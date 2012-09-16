/*
   The bGeigie
   A device for car-borne radiation measurement (aka Radiation War-driving).

   This code is for the single-board bGeigie designed for Safecast.

   Copyright (c) 2011, Robin Scheibler aka FakuFaku, Christopher Wang aka Akiba
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the <organization> nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// C libraries
#include <limits.h>

// Arduino libraries
#include <SD.h>
#include <SPI.h>
#include <EEPROM.h>

// 3rd party Arduino libraries
#include <chibi.h>
#include <Cmd.h>

// Safecast bGeigie library
#include <bg3_pins.h>
#include <GPS.h>
#include <HardwareCounter.h>
#include <sd_logger.h>
#include <bg_sensors.h>
#include <bg_pwr.h>
#include <sd_reader_int.h>

// version header
#include "version.h"

#define TIME_INTERVAL 5000
#define NX 12
#define AVAILABLE 'A'          // indicates geiger data are ready (available)
#define VOID      'V'          // indicates geiger data not ready (void)
#define BMRDD_EEPROM_ID 100
#define BMRDD_ID_LEN 3

// Radio options
#define DEST_ADDR 0xFFFF      // this is the 802.15.4 broadcast address
#define CHANNEL 20            // transmission channel

// Enable or Disable features
#define RADIO_ENABLE 1
#define SD_READER_ENABLE 1
#define BG_PWR_ENABLE 1
#define CMD_LINE_ENABLE 1
#define HVPS_SENSING 0


// Messages and Errors in Flash
static char msg_device_id[] PROGMEM = "Device id: ";

// Geiger rolling and total count
unsigned long shift_reg[NX] = {0};
unsigned long reg_index = 0;
unsigned long total_count = 0;
int str_count = 0;
char geiger_status = VOID;

// Hardware counter
static HardwareCounter hwc(counts, TIME_INTERVAL);

// the line buffer for serial receive and send
static char line[LINE_SZ];

char filename[13];              // placeholder for filename
char hdr[] = "BNXRDD";         // BGeigie New RaDiation Detector header
char hdr_status[] = "BNXSTS";  // Status message header
char dev_id[BMRDD_ID_LEN+1];    // device id
char ext_log[] = ".log";
char ext_sts[] = ".sts";
 
// State variables
int gps_init_acq = 0;
int log_created = 0;

/* SETUP */
void setup()
{
  char tmp[25];

  // init led
  bg_led_config();
  bg_led_off();
  
  // init serial
  Serial.begin(57600);
  Serial.println("Welcome to bGeigie");

  // initialize GPS using second Serial connection
  Serial1.begin(9600);
  gps_init(&Serial1, line);
  bg_gps_pwr_config();
  bg_gps_on();

  Serial.println("Wait for GPS to start.");
  int t = 0;
  while(!Serial1.available())
  {
    delay(10);
    t++;
  }
  Serial.print("GPS started in less than ");
  Serial.print(t*10);
  Serial.println("ms.");

  // Issue some commands to the GPS
  Serial1.println(MTK_SET_NMEA_OUTPUT_RMCGGA);
  Serial1.println(MTK_UPDATE_RATE_1HZ);

  // initialize SD card
  sd_log_init(sd_pwr, sd_detect, cs_sd);

  // initialize sensors
  bgs_sensors_init(sense_pwr, batt_sense, temp_sense, hum_sense, hv_sense);
  
  // set device id
  pullDevId();

  strcpy_P(tmp, msg_device_id);
  Serial.print(tmp);
  Serial.println(dev_id);

#if RADIO_ENABLE
  // init chibi on channel normally 20
  chibiInit();
  chibiSetChannel(CHANNEL);
#endif

  // set initial state of Geiger to void
  geiger_status = VOID;
  str_count = 0;
  
  // print free RAM. we should try to maintain about 300 bytes for stack space
  Serial.println(FreeRam());

  // ***WARNING*** turn High Voltage board ON ***WARNING***
  bg_hvps_pwr_config();
  bg_hvps_on();

  // And now Start the Pulse Counter!
  hwc.start();

  // setup command line commands
#if CMD_LINE_ENABLE
  cmdAdd("help", cmdPrintHelp);
  cmdAdd("setid", cmdSetId);
  cmdAdd("getid", cmdGetId);
#endif 

  // setup the SD reader
#if SD_READER_ENABLE
  sd_reader_setup();
#endif

  // setup power management
#if BG_PWR_ENABLE
  bg_pwr_init(main_switch, power_up, shutdown);
#endif

  // Starting now!
  Serial.println("Starting now!");

  // delay to give time to serial to finish
  delay(10);

}

/* MAIN LOOP */
void loop()
{
  char tmp[25];

#if BG_PWR_ENABLE
  // power management loop routine
  bg_pwr_loop();
#endif

#if SD_READER_ENABLE
  // First do the SD Reader loop
  sd_reader_loop();
#endif

  // We lock the SD reader if possible when executing the loop
#if SD_READER_ENABLE && BG_PWR_ENABLE
  if (bg_pwr_running() && sd_reader_lock())
#elif SD_READER_ENABLE
  if (sd_reader_lock())
#endif
  {

#if CMD_LINE_ENABLE
    // command line loop polling routing
    cmdPoll();
#endif

    // update gps
    gps_update();

    // generate CPM every TIME_INTERVAL seconds
    if (hwc.available())
    {
      if (gps_available())
      {
        unsigned long cpm=0, cpb=0;
        byte line_len;

        // obtain the count in the last bin
        cpb = hwc.count();

        // reset the pulse counter
        hwc.start();

        // insert count in sliding window and compute CPM
        shift_reg[reg_index] = cpb;     // put the count in the correct bin
        reg_index = (reg_index+1) % NX; // increment register index
        cpm = cpm_gen();                // compute sum over all bins

        // update the total counter
        total_count += cpb;
        
        // set status of Geiger
        if (str_count < NX)
        {
          geiger_status = VOID;
          str_count++;
        } else if (cpm == 0) {
          geiger_status = VOID;
        } else {
          geiger_status = AVAILABLE;
        }


        if (gps_init_acq == 0 && gps_getData()->status[0] == AVAILABLE)
        {
          // flag GPS acquired
          gps_init_acq = 1;

          // Create the filename for that drive
          strcpy(filename, dev_id);
          strcat(filename, "-");
          strncat(filename, gps_getData()->date+2, 2);
          strncat(filename, gps_getData()->date, 2);

          // Prepare new log file header
          sprintf(line, "# bGeigie - v%s\n# New log begin", version);

          // write to log file on SD card
          strcpy(filename+8, ext_log);
          sd_log_writeln(filename, line);

          // write to status file as well
          strcpy(filename+8, ext_sts);
          sd_log_writeln(filename, line);
        }
        
        // generate timestamp. only update the start time if 
        // we printed the timestamp. otherwise, the GPS is still 
        // updating so wait until its finished and generate timestamp
        line_len = gps_gen_timestamp(line, shift_reg[reg_index], cpm, cpb);
        
        if (gps_init_acq == 0)
        {
          Serial.print("No GPS: ");
          sd_log_last_write = 0;   // because we don't write to SD before GPS lock
        }
        else
        {
          // dump data to SD card
          strcpy(filename+8, ext_log);
          sd_log_writeln(filename, line);
        }

#if RADIO_ENABLE
        // send out wirelessly. first wake up the radio, do the transmit, then go back to sleep
        chibiSleepRadio(0);
        delay(10);
        chibiTx(DEST_ADDR, (byte *)line, LINE_SZ);
        chibiSleepRadio(1);
#endif

        // blink if log written
        if (sd_log_last_write == 1)
        {
          if (gps_getData()->status[0] == AVAILABLE)
            blink_led(1, 100);
          else
            blink_led(2, 100);
        }

        // output through Serial too
        Serial.println(line);

        // Now take care of the Status message
        line_len = bg_status_str_gen(line);

        // write to SD status file
        if (gps_init_acq != 0)
        {
          strcpy(filename+8, ext_sts);
          sd_log_writeln(filename, line);
        }

#if RADIO_ENABLE
        // send out wirelessly. first wake up the radio, do the transmit, then go back to sleep
        chibiSleepRadio(0);
        delay(10);
        chibiTx(DEST_ADDR, (byte *)line, LINE_SZ);
        chibiSleepRadio(1);
  #endif
      
        // show in Serial stream
        Serial.println(line);

      } /* gps_available */
    } /* hwc_available */

  } /* sd_reader_lock */
#if SD_READER_ENABLE
  else
  {
    // always restart the pulse counter when in SD reader mode
    // that way pulse count doesn't accumulate while being in
    // SD reader mode.
    hwc.start();
  }
  // Unlock the SD reader after the loop
  sd_reader_unlock();
#endif

}

/* generate log line */
byte gps_gen_timestamp(char *buf, unsigned long counts, unsigned long cpm, unsigned long cpb)
{
  byte len;
  byte chk;

  gps_t *ptr = gps_getData();
  
  memset(buf, 0, LINE_SZ);
  sprintf_P(buf, PSTR("$%s,%s,20%s-%s-%sT%s:%s:%sZ,%ld,%ld,%ld,%c,%s,%s,%s,%s,%s,%s,%s,%s,%s"),  \
              hdr, \
              dev_id, \
              ptr->datetime.year, ptr->datetime.month, ptr->datetime.day,  \
              ptr->datetime.hour, ptr->datetime.minute, ptr->datetime.second, \
              cpm, \
              cpb, \
              total_count, \
              geiger_status, \
              ptr->lat, ptr->lat_hem, \
              ptr->lon, ptr->lon_hem, \
              ptr->altitude, \
              ptr->status, \
              ptr->num_sat, \
              ptr->precision, \
              ptr->quality);
   len = strlen(buf);
   buf[len] = '\0';

   // generate checksum
   chk = gps_checksum(buf+1, len);

   // add checksum to end of line before sending
   if (chk < 16)
     sprintf(buf + len, "*0%X", (int)chk);
   else
     sprintf(buf + len, "*%X", (int)chk);

   return len;
}

/* create Status log line */
byte bg_status_str_gen(char *buf)
{
  byte len;
  byte chk;

  int batt, hv;
  float t, h;
  int tf, hf;

  // turn sensors on
  bg_sensors_on();
  delay(100);

  // read all sensors
  hv = bgs_read_hv();        // V
  batt = 1000*bgs_read_battery(); // mV

  // need to compute fractional part separately because sprintf doesn't support it
  t = bgs_read_temperature();
  tf = (int)((t-(int)t)*100);
  h = bgs_read_humidity();
  hf = (int)((h-(int)h)*100);

  // turn sensors off
  bg_sensors_off();

  // get GPS data
  gps_t *ptr = gps_getData();

  // create string
  memset(buf, 0, LINE_SZ);
#if HVPS_SENSING
  sprintf_P(buf, PSTR("$%s,%s,20%s-%s-%sT%s:%s:%sZ,%s,%s,%s,%s,v%s,%d,%d,%d,%d,%d,%d,%d"),  \
              hdr_status, \
              dev_id, \
              ptr->datetime.year, ptr->datetime.month, ptr->datetime.day,  \
              ptr->datetime.hour, ptr->datetime.minute, ptr->datetime.second, \
              ptr->lat, ptr->lat_hem, \
              ptr->lon, ptr->lon_hem, \
              version, \
              (int)t,  \
              (int)h,  \
              batt, \
              hv, \
              sd_log_inserted, sd_log_initialized, sd_log_last_write);
#else
  sprintf_P(buf, PSTR("$%s,%s,20%s-%s-%sT%s:%s:%sZ,%s,%s,%s,%s,v%s,%d,%d,%d,,%d,%d,%d"),  \
              hdr_status, \
              dev_id, \
              ptr->datetime.year, ptr->datetime.month, ptr->datetime.day,  \
              ptr->datetime.hour, ptr->datetime.minute, ptr->datetime.second, \
              ptr->lat, ptr->lat_hem, \
              ptr->lon, ptr->lon_hem, \
              version, \
              (int)t,  \
              (int)h,  \
              batt, \
              sd_log_inserted, sd_log_initialized, sd_log_last_write);
#endif
  len = strlen(buf);
  buf[len] = '\0';

  // generate checksum
  chk = gps_checksum(buf+1, len);

  // add checksum to end of line before sending
  if (chk < 16)
    sprintf(buf + len, "*0%X", (int)chk);
  else
    sprintf(buf + len, "*%X", (int)chk);

  return len;
}

/* compute total of rolling count */
unsigned long cpm_gen()
{
   unsigned int i;
   unsigned long c_p_m = 0;
   
   // sum up
   for (i=0 ; i < NX ; i++)
     c_p_m += shift_reg[i];
   
   return c_p_m;
}

/* read device id from EEPROM */
void pullDevId()
{
  // counter for trials of reading EEPROM
  int n = 0;
  int N = 3;

  cli(); // disable all interrupts

  for (int i=0 ; i < BMRDD_ID_LEN ; i++)
  {
    // try to read one time
    dev_id[i] = (char)EEPROM.read(i+BMRDD_EEPROM_ID);
    n = 1;
    // while it's not numberic, and up to N times, try to reread.
    while ((dev_id[i] < '0' || dev_id[i] > '9') && n < N)
    {
      // wait a little before we retry
      delay(10);        
      // reread once and then increment the counter
      dev_id[i] = (char)EEPROM.read(i+BMRDD_EEPROM_ID);
      n++;
    }

    // catch when the read number is non-numeric, replace with capital X
    if (dev_id[i] < '0' || dev_id[i] > '9')
      dev_id[i] = 'X';
  }

  // set the end of string null
  dev_id[BMRDD_ID_LEN] = NULL;

  sei(); // re-enable all interrupts
}


/***********************************/
/* Power on and shutdown functions */
/***********************************/

void power_up()
{
  char tmp[25];

  // blink
  blink_led(3, 100);

  bg_gps_on();
  sd_log_pwr_on();
  bg_sensors_on();
  
  // set initial state of Geiger to void
  geiger_status = VOID;
  str_count = 0;
  
  // ***WARNING*** turn High Voltage board ON ***WARNING***
  bg_hvps_on();
  delay(50); // wait for power to stabilize

  // And now Start the Pulse Counter!
  hwc.start();

  // Starting now!
  Serial.println("bGeigie powered on!");
}

void shutdown()
{
  // indicate something is happening
  blink_led(3, 100);

  bg_gps_off();
  sd_log_pwr_off();
  chibiSleepRadio(1);
  bg_hvps_off();
  bg_sensors_off();
  bg_led_off();

  // we turn all pins to input no pull-up to save power
  // this will also turn off all the peripherals
  // automatically via external pull-up resistors
  /*
  for (int i=0 ; i < NUM_DIGITAL_PINS ; i++)
  {
    digitalWrite(i, LOW);
    pinMode(i, INPUT);
  }
  */

  // say goodbye...
  Serial.println("bGeigie sleeps... good night.");
  delay(20);
}


/**************************/
/* command line functions */
/**************************/

void printHelp()
{
  Serial.print("Device id: ");
  Serial.println(dev_id);
  Serial.println("List of commands:");
  Serial.print("  Set new device id: setid <id>    id is ");
  Serial.print(BMRDD_ID_LEN);
  Serial.println(" characters long");
  Serial.println("  Get device id:     getid");
  Serial.println("  Show this help:    help");
}

void cmdPrintHelp(int arg_cnt, char **args)
{
  printHelp();
}  

/* set new device id in EEPROM */
void cmdSetId(int arg_cnt, char **args)
{
  if (arg_cnt != 2)
  {
    Serial.print("Synthax: setid <id>    id is ");
    Serial.print(BMRDD_ID_LEN);
    Serial.println(" characters long");
    return;
  }

  int len = 0;
  while (args[1][len] != NULL)
    len++;

  if (len != BMRDD_ID_LEN)
  {
    Serial.print("Synthax: setid <id>     id is ");
    Serial.print(BMRDD_ID_LEN);
    Serial.println(" characters long");
    return;
  } 

  for (int i=0 ; i < BMRDD_ID_LEN ; i++)
  {
    EEPROM.write(BMRDD_EEPROM_ID+i, byte(args[1][i]));
    dev_id[i] = args[1][i];
  }
  Serial.print("Device id: ");
  Serial.println(dev_id);
}

void cmdGetId(int arg_cnt, char **args)
{
  pullDevId();
  Serial.print("Device id: ");
  Serial.println(dev_id);
}


/********/
/* misc */
/********/

void blink_led(unsigned int N, unsigned int D)
{
  for (int i=0 ; i < N ; i++)
  {
    bg_led_on();
    delay(D);
    bg_led_off();
    delay(D);
  }
}
