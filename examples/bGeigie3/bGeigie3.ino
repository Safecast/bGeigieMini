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

#define BATT_LOWEST_VOLTAGE 3300  // lowest voltage acceptable by power regulator

// Radio options
#define DEST_ADDR 0xFFFF      // this is the 802.15.4 broadcast address
#define RX_ADDR_BASE 0x3000   // base for radio address. Add device number to make complete address
#define CHANNEL 20            // transmission channel

// Enable or Disable features
#define RADIO_ENABLE 1
#define SD_READER_ENABLE 1
#define BG_PWR_ENABLE 1
#define CMD_LINE_ENABLE 1
#define HVPS_SENSING 0
#define GPS_1PPS_ENABLE 0
#define JAPAN_POST 0


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
 
// State variables
int rtc_acq = 0;
int log_created = 0;

// radio variables
#if RADIO_ENABLE
uint8_t radio_init_status = 0;
unsigned int radio_addr = RX_ADDR_BASE;
#endif

// SD reader variables
#if SD_READER_ENABLE
uint8_t sd_reader_init_status = 0;
#endif

// Enable Serial output
uint8_t serial_output_enable = 0;

// a function to initialize all global variables when the device starts
void global_variables_init()
{
  rtc_acq = 0;
  log_created = 0;

  for (int i = 0 ; i < NX ; i++)
    shift_reg[i] = 0;
  reg_index = 0;

  total_count = 0;
  str_count = 0;
  geiger_status = VOID;

  serial_output_enable = 0;
}

/* SETUP */
void setup()
{
  char tmp[30];

  // init led
  bg_led_config();
  bg_led_off();
  
  // init serial
  Serial.begin(57600);
  strcpy_P(tmp, PSTR("*** Welcome to bGeigie ***"));
  Serial.println(tmp);
  strcpy_P(tmp, PSTR("Version-"));
  Serial.print(tmp);
  Serial.println(version);

  // initialize GPS using second Serial connection
  Serial1.begin(9600);
  gps_init(&Serial1, line);
  bg_gps_pwr_config();
  bg_gps_on();

  int t = 0;
  while(!Serial1.available() && t < 100)
  {
    delay(10);
    t++;
  }
  strcpy_P(tmp, PSTR("GPS start time,"));
  Serial.print(tmp);
  if (t < 100)
  {
    Serial.print(t*10);
    strcpy_P(tmp, PSTR("ms"));
    Serial.println(tmp);
  }
  else
  {
    strcpy_P(tmp, PSTR("timeout"));
    Serial.println(tmp);
  }

  // Initialize interrupt on GPS 1PPS line
#if GPS_1PPS_ENABLE  
  pinMode(gps_1pps, INPUT);
  BG_1PPS_INTP();
#endif

  // Issue some commands to the GPS
  Serial1.println(MTK_SET_NMEA_OUTPUT_RMCGGA);
  Serial1.println(MTK_UPDATE_RATE_1HZ);

  // initialize SD card
  sd_log_init(sd_pwr, sd_detect, cs_sd);

  // initialize sensors
  bgs_sensors_init(sense_pwr, batt_sense, temp_sense, hum_sense, hv_sense);
  
  // setup the SD reader
#if SD_READER_ENABLE
  sd_reader_init_status = sd_reader_setup();
#endif

  // set device id
  pullDevId();

#if RADIO_ENABLE
  // init chibi on channel normally 20
  radio_init_status = chibiInit();  // init the radio
  chibiSetChannel(CHANNEL); // choose the channel
  radio_addr = getRadioAddr();    // generate radio address based on device id
  chibiSetShortAddr(radio_addr);  // set the address
  chibiSleepRadio(1);       // sleep the radio
#endif

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
  cmdAdd("verbose", cmdVerbose);
#endif 

  // setup power management
#if BG_PWR_ENABLE
  bg_pwr_init(main_switch, power_up, shutdown);
#endif

  // initialize all global variables
  global_variables_init();

  // run all diagnostics
  diagnostics();

  // Starting now!
  Serial.println("Starting now!");

  // delay to give time to serial to finish
  delay(10);

}

/* MAIN LOOP */
void loop()
{
  char tmp[25];

  // we check the voltage of the battery and turn
  // the device off if it lower than the lowest
  // voltage acceptable by the power regulator (3.3V)
  check_battery_voltage();

#if BG_PWR_ENABLE
  // power management loop routine
  bg_pwr_loop();
#endif

#if SD_READER_ENABLE
  // First do the SD Reader loop
  if (sd_reader_init_status)  // but only if it initialized properly
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


        // check the RTC is correct
        // by default, we check that the year is not 1980 (default GPS module year)
        // obviously this won't work past 2079
        // to ensure GPS will work in the year 2080, we also condition on fix status
        // in every year other than xx80, the system starts recording when year is not '80' (i.e. RTC running)
        // in xx80, it only starts when a fix is acquired.
        if (rtc_acq == 0 && ( gps_getData()->status[0] == 'A' || strncmp(gps_getData()->datetime.year, "80", 2) != 0) )
        {
          // flag GPS acquired
          rtc_acq = 1;

          // Create the filename for that drive
          strcpy(filename, dev_id);
          strcat(filename, "-");
          strncat(filename, gps_getData()->date+2, 2);
          strncat(filename, gps_getData()->date, 2);

          // write to log file on SD card
          strcpy(filename+8, ext_log);
          writeHeader2SD(filename);
        }
        
        // generate timestamp. only update the start time if 
        // we printed the timestamp. otherwise, the GPS is still 
        // updating so wait until its finished and generate timestamp
        line_len = gps_gen_timestamp(line, shift_reg[reg_index], cpm, cpb);
        
        if (rtc_acq == 0)
        {
          sd_log_last_write = 0;   // because we don't write to SD before GPS lock
          if (serial_output_enable)
            Serial.print("No GPS: ");
        }
        else
        {
          // dump data to SD card
          strcpy(filename+8, ext_log);
          sd_log_writeln(filename, line);
        }

#if RADIO_ENABLE
        // send out wirelessly. first wake up the radio, do the transmit, then go back to sleep
        if (radio_init_status)  // but only if it initialized properly
        {
          chibiSleepRadio(0);
          delay(10);
          chibiTx(DEST_ADDR, (byte *)line, LINE_SZ);
          chibiSleepRadio(1);
        }
#endif

        // blink if log written
        if (sd_log_last_write == 1)
        {
          if (gps_getData()->status[0] == AVAILABLE)
            blink_led(1, 100);  // blink once when GPS acquired
          else
            blink_led(2, 100);  // blink twice when GPS not available
        }

        // output through Serial too
        if (serial_output_enable)
          Serial.println(line);

        // Now take care of the Status message
        line_len = bg_status_str_gen(line);

        // write to SD status file
        if (rtc_acq != 0)
        {
          strcpy(filename+8, ext_log);
          //sd_log_pwr_on();
          sd_log_writeln(filename, line);
          //sd_log_pwr_off();
        }

#if RADIO_ENABLE
        // send out wirelessly. first wake up the radio, do the transmit, then go back to sleep
        if (radio_init_status)  // but only if it initialized properly
        {
          chibiSleepRadio(0);
          delay(10);
          chibiTx(DEST_ADDR, (byte *)line, LINE_SZ);
          chibiSleepRadio(1);
        }
  #endif
      
        // show in Serial stream
        if (serial_output_enable)
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

#if JAPAN_POST
  // if for JP, make sure to truncate the GPS coordinates
  truncate_JP(ptr->lat, ptr->lon);
#endif
  
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
  h = bgs_read_humidity();

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

/* Set the address for the radio based on device ID */
unsigned int getRadioAddr()
{
  // prefix with two so that it never collides with broadcast address
  unsigned int a = RX_ADDR_BASE;

  // first digit
  if (dev_id[0] != 'X')
    a += (unsigned int)(dev_id[0]-'0') * 0x100;

  // second digit
  if (dev_id[1] != 'X')
    a += (unsigned int)(dev_id[1]-'0') * 0x10;

  // third digit
  if (dev_id[2] != 'X')
    a += (unsigned int)(dev_id[2]-'0') * 0x1;

  return a;
}


/***********************************/
/* Power on and shutdown functions */
/***********************************/

void power_up()
{
  char tmp[25];

  // blink if power up (not SD reader)
  if (!sd_reader_interrupted)
    blink_led(3, 100);

  pinMode(cs_sd, OUTPUT);
  digitalWrite(cs_sd, HIGH);
  pinMode(MOSI, OUTPUT);
  pinMode(SCK, OUTPUT);

  // flush Serial1 (GPS) before restarting GPS
  while(Serial1.available())
    Serial1.read();

  // turn GPS on and set status to not acquired yet
  bg_gps_on();

  // turn SD on and initialize
  sd_log_pwr_on();
  sd_log_init(sd_pwr, sd_detect, cs_sd);

  // initialize sensors
  bg_sensors_on();

#if SD_READER_ENABLE
  // initialize SD reader
  sd_reader_init_status = sd_reader_setup();
#endif

  // initialize all global variables
  // set initial states of GPS, log file, Geiger counter, etc.
  global_variables_init();

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

  digitalWrite(cs_sd, LOW);
  pinMode(cs_sd, INPUT);
  digitalWrite(MOSI, LOW);
  pinMode(MOSI, INPUT);
  digitalWrite(SCK, LOW);
  pinMode(SCK, INPUT);

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

void check_battery_voltage()
{
  
  int batt = 1000*bgs_read_battery(); // mV
  if (batt < BATT_LOWEST_VOLTAGE)
  {
    Serial.println("Battery level too low. Turning off.");
    bg_pwr_turn_off();
  }
}

/*************************/
/* Write Options to File */
/*************************/

void writeHeader2SD(char *filename)
{
  char tmp[50];

  strcpy_P(tmp, PSTR("# Welcome to Safecast bGeigie3 System"));
  sd_log_writeln(filename, tmp);

  strcpy_P(tmp, PSTR("# Version,"));
  strcat(tmp, version);
  sd_log_writeln(filename, tmp);

  // System free RAM
  sprintf_P(tmp, PSTR("# System free RAM,%dB"), FreeRam());
  sd_log_writeln(filename, tmp);

#if RADIO_ENABLE
  strcpy_P(tmp, PSTR("# Radio enabled,yes"));
#else
  strcpy_P(tmp, PSTR("# Radio enabled,no"));
#endif
  sd_log_writeln(filename, tmp);

#if SD_READER_ENABLE
  strcpy_P(tmp, PSTR("# SD reader enabled,yes"));
#else
  strcpy_P(tmp, PSTR("# SD reader enabled,no"));
#endif
  sd_log_writeln(filename, tmp);

#if BG_PWR_ENABLE
  strcpy_P(tmp, PSTR("# Power management enabled,yes"));
#else
  strcpy_P(tmp, PSTR("# Power management enabled,no"));
#endif
  sd_log_writeln(filename, tmp);

#if CMD_LINE_ENABLE
  strcpy_P(tmp, PSTR("# Command line interface enabled,yes"));
#else
  strcpy_P(tmp, PSTR("# Command line interface enabled,no"));
#endif
  sd_log_writeln(filename, tmp);

#if HVPS_SENSING
  strcpy_P(tmp, PSTR("# HV sense enabled,yes"));
#else
  strcpy_P(tmp, PSTR("# HV sense enabled,no"));
#endif
  sd_log_writeln(filename, tmp);

#if GPS_1PPS_ENABLE
  strcpy_P(tmp, PSTR("# GPS 1PPS enabled,yes"));
#else
  strcpy_P(tmp, PSTR("# GPS 1PPS enabled,no"));
#endif
  sd_log_writeln(filename, tmp);

#if JAPAN_POST
  strcpy_P(tmp, PSTR("# Coordinate truncation enabled,yes"));
  sd_log_writeln(filename, tmp);
#else
  strcpy_P(tmp, PSTR("# Coordinate truncation enabled,no"));
  sd_log_writeln(filename, tmp);
#endif

}

/**********************/
/* Diagnostic Routine */
/**********************/

void diagnostics()
{
  char tmp[50];

  strcpy_P(tmp, PSTR("--- Diagnostic START ---"));
  Serial.println(tmp);

  // Version number
  strcpy_P(tmp, PSTR("Version,"));
  Serial.print(tmp);
  Serial.println(version);

  // Device ID
  strcpy_P(tmp, PSTR("Device ID,"));
  Serial.print(tmp);
  Serial.println(dev_id);

#if RADIO_ENABLE
  // basic radio operations
  strcpy_P(tmp, PSTR("Radio enabled,yes"));
  Serial.println(tmp);
  strcpy_P(tmp, PSTR("Radio initialized,"));
  Serial.print(tmp);
  if (radio_init_status)
  {
    strcpy_P(tmp, PSTR("yes"));
    Serial.println(tmp);
  }
  else
  {
    strcpy_P(tmp, PSTR("no"));
    Serial.println(tmp);
  }
  chibiSleepRadio(0);
  strcpy_P(tmp, PSTR("Radio address,"));
  Serial.print(tmp);
  Serial.println(chibiGetShortAddr(), HEX);
  strcpy_P(tmp, PSTR("Radio channel,"));
  Serial.print(tmp);
  Serial.println(chibiGetChannel());
  chibiSleepRadio(1);
#else
  strcpy_P(tmp, PSTR("Radio enabled,no"));
  Serial.println(tmp);
#endif

  // GPS
  gps_diagnostics();

  // SD card
  //sd_log_pwr_on();
  sd_log_card_diagnostic();  
  //sd_log_pwr_off();

#if SD_READER_ENABLE
  // SD reader
  strcpy_P(tmp, PSTR("SD reader enabled,yes"));
  Serial.println(tmp);
  strcpy_P(tmp, PSTR("SD reader initialized,"));
  Serial.print(tmp);
  if (sd_reader_init_status)
  {
    strcpy_P(tmp, PSTR("yes"));
    Serial.println(tmp);
  }
  else
  {
    strcpy_P(tmp, PSTR("no"));
    Serial.println(tmp);
  }
#else
  strcpy_P(tmp, PSTR("SD reader enabled,no"));
  Serial.println(tmp);
#endif

  // turn sensors on
  bg_sensors_on();
  delay(100);

  // temperature
  int t = bgs_read_temperature();
  t = bgs_read_temperature();
  strcpy_P(tmp, PSTR("Temperature,"));
  Serial.print(tmp);
  Serial.print(t);
  Serial.println('C');
  
  //humidity
  int h = bgs_read_humidity();  // %
  h = bgs_read_humidity();      // read second time to get stable reading
  strcpy_P(tmp, PSTR("Humidity,"));
  Serial.print(tmp);
  Serial.print(h);
  Serial.println('%');

  // turn sensors on
  bg_sensors_off();

  // battery voltage
  int batt = 1000*bgs_read_battery(); // mV
  batt = 1000*bgs_read_battery();     // read second time to get stable reading
  strcpy_P(tmp, PSTR("Battery voltage,"));
  Serial.print(tmp);
  Serial.print(batt);
  strcpy_P(tmp, PSTR("mV"));
  Serial.println(tmp);

  // HV sensing
#if HVPS_SENSING
  strcpy_P(tmp, PSTR("HV sense enabled,yes"));
  Serial.println(tmp);
  // read all sensors
  int hv = bgs_read_hv();  // V
  hv = bgs_read_hv(); // read a second time to get rid of bias, or something
  strcpy_P(tmp, PSTR("High voltage power supply,"));
  Serial.print(tmp);
  Serial.print(hv);
  strcpy_P(tmp, PSTR("V"));
  Serial.println(tmp);
#else
  strcpy_P(tmp, PSTR("HV sense enabled,no"));
  Serial.println(tmp);
#endif

  // System free RAM
  strcpy_P(tmp, PSTR("System free RAM,"));
  Serial.print(tmp);
  Serial.print(FreeRam());
  Serial.println('B');

#if BG_PWR_ENABLE
  strcpy_P(tmp, PSTR("Power management enabled,yes"));
  Serial.println(tmp);
#else
  strcpy_P(tmp, PSTR("Power management enabled,no"));
  Serial.println(tmp);
#endif

#if CMD_LINE_ENABLE
  strcpy_P(tmp, PSTR("Command line interface enabled,yes"));
  Serial.println(tmp);
#else
  strcpy_P(tmp, PSTR("Command line interface enabled,no"));
  Serial.println(tmp);
#endif

#if GPS_1PPS_ENABLE
  strcpy_P(tmp, PSTR("GPS 1PPS enabled,yes"));
  Serial.println(tmp);
#else
  strcpy_P(tmp, PSTR("GPS 1PPS enabled,no"));
  Serial.println(tmp);
#endif

#if JAPAN_POST
  strcpy_P(tmp, PSTR("Coordinate truncation enabled,yes"));
  Serial.println(tmp);
#else
  strcpy_P(tmp, PSTR("Coordinate truncation enabled,no"));
  Serial.println(tmp);
#endif

  
  strcpy_P(tmp, PSTR("--- Diagnostic END ---"));
  Serial.println(tmp);
}

/**************************/
/* command line functions */
/**************************/

void printHelp()
{
  char tmp[50];
  strcpy_P(tmp, PSTR("Device id: "));
  Serial.println(tmp);
  Serial.println(dev_id);
  strcpy_P(tmp, PSTR("List of commands:"));
  Serial.println(tmp);
  strcpy_P(tmp, PSTR("  Set new device id:     setid <id>    id is "));
  Serial.print(tmp);
  Serial.print(BMRDD_ID_LEN);
  strcpy_P(tmp, PSTR(" characters long"));
  Serial.println(tmp);
  strcpy_P(tmp, PSTR("  Get device id:         getid"));
  Serial.println(tmp);
  strcpy_P(tmp, PSTR("  Enable log to serial:  verbose on"));
  Serial.println(tmp);
  strcpy_P(tmp, PSTR("  Disable log to serial: verbose off"));
  Serial.println(tmp);
  strcpy_P(tmp, PSTR("  Show this help:        help"));
  Serial.println(tmp);
}

void cmdPrintHelp(int arg_cnt, char **args)
{
  printHelp();
}  

void cmdVerbose(int arg_cnt, char **args)
{
  if (arg_cnt != 2)
    goto error;

  if (strncmp_P(args[1], PSTR("on"), 2) == 0)
  {
    serial_output_enable = 1;
    return;
  }
  else if (strncmp_P(args[1], PSTR("off"), 3) == 0)
  {
    serial_output_enable = 0;
    return;
  }

error:
  char tmp[30];
  strcpy_P(tmp, "Synthax: verbose <on/off>");
  Serial.println(tmp);
  return;
}

/* set new device id in EEPROM */
void cmdSetId(int arg_cnt, char **args)
{
  char tmp[40];
  int len = 0;

  if (arg_cnt != 2)
    goto errorSetId;

  while (args[1][len] != NULL)
    len++;

  if (len != BMRDD_ID_LEN)
    goto errorSetId;

  // write ID to EEPROM
  for (int i=0 ; i < BMRDD_ID_LEN ; i++)
    EEPROM.write(BMRDD_EEPROM_ID+i, byte(args[1][i]));

  // pull dev id from the EEPROM so that we check it was successfully written
  pullDevId();
  strcpy_P(tmp, PSTR("Device id: "));
  Serial.print(tmp);
  Serial.print(dev_id);
  if (strncmp(dev_id, args[1], BMRDD_ID_LEN) == 0)
    strcpy_P(tmp, PSTR(" - success."));
  else
    strcpy_P(tmp, PSTR(" - failure."));
  Serial.println(tmp);
    
  return; // happyily

errorSetId:
  strcpy_P(tmp, PSTR("Synthax: setid <id>     id is "));
  Serial.print(tmp);
  Serial.print(BMRDD_ID_LEN);
  strcpy_P(tmp, PSTR(" characters long"));
  Serial.println(tmp);
}

void cmdGetId(int arg_cnt, char **args)
{
  pullDevId();
  Serial.print("Device id: ");
  Serial.println(dev_id);
}


/**********************/
/* GPS 1PPS Interrupt */
/**********************/

#if GPS_1PPS_ENABLE
ISR(BG_1PPS_INT)
{
  int val = *portInputRegister(digitalPinToPort(gps_1pps)) & digitalPinToBitMask(gps_1pps);
  if (val)
  {
    // make good use of GPS 1PPS here
    rtc_pulse++;
  }
}
#endif

/**********************/
/* JP truncation code */
/**********************/

#if JAPAN_POST
/* 
 * Truncate the latitude and longitude according to
 * Japan Post requirements
 * 
 * This algorithm truncate the minute
 * part of the latitude and longitude
 * in order to rasterize the points on
 * a 100x100m grid.
 */
void truncate_JP(char *lat, char *lon)
{
  unsigned long minutes;
  float lat0;
  unsigned int lon_trunc;

  /* latitude */
  // get minutes in one long int
  minutes =  (unsigned long)(lat[2]-'0')*100000 
    + (unsigned long)(lat[3]-'0')*10000 
    + (unsigned long)(lat[5]-'0')*1000 
    + (unsigned long)(lat[6]-'0')*100 
    + (unsigned long)(lat[7]-'0')*10 
    + (unsigned long)(lat[8]-'0');
  // truncate, for latutude, truncation factor is fixed
  minutes -= minutes % 546;
  // get this back in the string
  lat[2] = '0' + (minutes/100000);
  lat[3] = '0' + ((minutes%100000)/10000);
  lat[5] = '0' + ((minutes%10000)/1000);
  lat[6] = '0' + ((minutes%1000)/100);
  lat[7] = '0' + ((minutes%100)/10);
  lat[8] = '0' + (minutes%10);

  // compute the full latitude in radian
  lat0 = ((float)(lat[0]-'0')*10 + (lat[1]-'0') + (float)minutes/600000.f)/180.*M_PI;

  /* longitude */
  // get minutes in one long int
  minutes =  (unsigned long)(lon[3]-'0')*100000 
    + (unsigned long)(lon[4]-'0')*10000 
    + (unsigned long)(lon[6]-'0')*1000 
    + (unsigned long)(lon[7]-'0')*100 
    + (unsigned long)(lon[8]-'0')*10 
    + (unsigned long)(lon[9]-'0');
  // compute truncation factor
  lon_trunc = (unsigned int)((0.0545674090600784/cos(lat0))*10000.);
  // truncate
  minutes -= minutes % lon_trunc;
  // get this back in the string
  lon[3] = '0' + (minutes/100000);
  lon[4] = '0' + ((minutes%100000)/10000);
  lon[6] = '0' + ((minutes%10000)/1000);
  lon[7] = '0' + ((minutes%1000)/100);
  lon[8] = '0' + ((minutes%100)/10);
  lon[9] = '0' + (minutes%10);

}
#endif /* JAPAN_POST */


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
