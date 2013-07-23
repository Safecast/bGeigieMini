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

// bGeigie specific headers
#include "config.h"
#include "commands.h"
#include "blinky.h"
#include "bg_pwr.h"
#include "sd_reader_int.h"

// version header
#include "version.h"

/* Available/Void labels for status values */
#define AVAILABLE 'A'          // indicates geiger data are ready (available)
#define VOID      'V'          // indicates geiger data not ready (void)

/* moving average length and number of bins (NX) */
#define TIME_INTERVAL 5000
#define NX 12

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

/* files name */
char filename[18];              // placeholder for filename
char ext_log[] = ".log";

/* log sentence header */
char hdr[] = "BNXRDD";         // BGeigie New RaDiation Detector header
char hdr_status[] = "BNXSTS";  // Status message header
 
// State variables
int rtc_acq = 0;
int log_created = 0;
unsigned int battery_voltage = 0;

// radio variables
#if RADIO_ENABLE
uint8_t radio_init_status = 0;
#endif

// SD reader variables
#if SD_READER_ENABLE
uint8_t sd_reader_init_status = 0;
#endif

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

  // Issue some commands to the GPS
  Serial1.println(MTK_SET_NMEA_OUTPUT_RMCGGA);
  Serial1.println(MTK_UPDATE_RATE_1HZ);

  // power up SD card
  pinMode(sd_pwr, OUTPUT);
  digitalWrite(sd_pwr, LOW);
  delay(20);

  // initialize SD card
  sd_log_init(sd_pwr, sd_detect, cs_sd);

  // initialize sensors
  bgs_sensors_init(sense_pwr, batt_sense, temp_sense, hum_sense, hv_sense);
  
  // setup the SD reader
#if SD_READER_ENABLE
  sd_reader_init_status = sd_reader_setup();
#endif

  // Initialize configuration
  config_init();

#if RADIO_ENABLE
  // init chibi on channel normally 20
  uint16_t radio_addr = RX_ADDR_BASE + (uint16_t)theConfig.id;  // generate radio address based on device id
  radio_init_status = chibiInit();  // init the radio
  chibiSetChannel(CHANNEL);         // choose the channel
  chibiSetShortAddr(radio_addr);    // set the address
  chibiSleepRadio(1);               // sleep the radio
#endif

  // ***WARNING*** turn High Voltage board ON ***WARNING***
  bg_hvps_pwr_config();
  bg_hvps_on();

  // And now Start the Pulse Counter!
  hwc.start();

  // setup command line commands
#if CMD_LINE_ENABLE
  registerCommands();
#endif 

  // setup power management
#if BG_PWR_ENABLE
  bg_pwr_init(main_switch, power_up, shutdown);
#endif

  // initialize all global variables
  global_variables_init();

  // switch LED on
  blinky(BLINK_ON);

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

    // manage LED blinking
    if (battery_voltage < BATT_LOW_VOLTAGE)
    {
      blinky(BLINK_BATTERY_LOW);
    }
    else if (!sd_log_last_write)
    {
      blinky(BLINK_PROBLEM);
    }
    else if (gps_getData()->status[0] == 'A')
    {
      blinky(BLINK_ALL_OK);
    }
    else
    {
      blinky(BLINK_ON);
    }

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
          strcpy(filename, "20");
          strncat(filename, gps_getData()->datetime.year, 2);

          // create the directory (if necessary)
          SD.mkdir(filename);

          // create the rest of the file name
          strcat(filename, "/");
          sprintf(filename + strlen(filename), "%x", theConfig.id & 0xFFF); // limit to 3 last digit
          strcat(filename, "-");
          strncat(filename, gps_getData()->datetime.month, 2);
          strncat(filename, gps_getData()->datetime.day, 2);
          strncat(filename, ext_log, 4);

          // write to log file on SD card
          writeHeader2SD(filename);
        }
        
        // truncate the GPS coordinates if the configuration says so (default disabled)
        if (theConfig.coord_truncation)
        {
          gps_t *ptr = gps_getData();
          truncate_JP(ptr->lat, ptr->lon);
        }
  
        // generate timestamp. only update the start time if 
        // we printed the timestamp. otherwise, the GPS is still 
        // updating so wait until its finished and generate timestamp
        line_len = gps_gen_timestamp(line, shift_reg[reg_index], cpm, cpb);
        
        if (rtc_acq == 0)
        {
          sd_log_last_write = 0;   // because we don't write to SD before GPS lock
          if (theConfig.serial_output)
            Serial.print("No GPS: ");
        }
        else
        {
          // dump data to SD card
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

        // output through Serial too
        if (theConfig.serial_output)
          Serial.println(line);

        // Now take care of the Status message
        line_len = bg_status_str_gen(line);

        // write to status to SD
        if (rtc_acq != 0)
          sd_log_writeln(filename, line);

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
        if (theConfig.serial_output)
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

    // also, we turn the LED off
    blinky(BLINK_OFF);
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
  sprintf_P(buf, PSTR("$%s,%lx,20%s-%s-%sT%s:%s:%sZ,%ld,%ld,%ld,%c,%s,%s,%s,%s,%s,%s,%s,%s,%s"),  \
              hdr, \
              (unsigned long)theConfig.id, \
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

  int batt, hv=0;
  float t, h;

  // turn sensors on
  bg_sensors_on();
  delay(100);

  // sense high voltage if configured so
  if (theConfig.hv_sense)
    hv = bgs_read_hv();        // V

  // sense battery voltage
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
  if (theConfig.hv_sense)
  {
    sprintf_P(buf, PSTR("$%s,%lx,20%s-%s-%sT%s:%s:%sZ,%s,%s,%s,%s,v%s,%d,%d,%d,%d,%d,%d,%d"),  \
        hdr_status, \
        (unsigned long)theConfig.id, \
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
  }
  else
  {
    sprintf_P(buf, PSTR("$%s,%lx,20%s-%s-%sT%s:%s:%sZ,%s,%s,%s,%s,v%s,%d,%d,%d,,%d,%d,%d"),  \
        hdr_status, \
        (unsigned long)theConfig.id, \
        ptr->datetime.year, ptr->datetime.month, ptr->datetime.day,  \
        ptr->datetime.hour, ptr->datetime.minute, ptr->datetime.second, \
        ptr->lat, ptr->lat_hem, \
        ptr->lon, ptr->lon_hem, \
        version, \
        (int)t,  \
        (int)h,  \
        batt, \
        sd_log_inserted, sd_log_initialized, sd_log_last_write);
  }
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


/***********************************/
/* Power on and shutdown functions */
/***********************************/

void power_up()
{

#if SD_READER_ENABLE
  if (!sd_reader_interrupted)
  {
#endif

    blinky(BLINK_ON);

    // Init SD card
    sd_log_init(sd_pwr, sd_detect, cs_sd);
#if SD_READER_ENABLE
    sd_reader_init_status = sd_reader_setup();
#endif

    // flush Serial1 (GPS) before restarting GPS
    while(Serial1.available())
      Serial1.read();

    // turn GPS on and set status to not acquired yet
    bg_gps_on();

    // initialize sensors
    bg_sensors_on();

    // re-init configuration after sleep
    config_init();

#if RADIO_ENABLE
    // init chibi on channel normally 20
    chibiSleepRadio(0);               // wake up the radio
    uint16_t radio_addr = RX_ADDR_BASE + (uint16_t)theConfig.id;  // generate radio address based on device id
    if (radio_addr != chibiGetShortAddr())
      chibiSetShortAddr(radio_addr);    // set the address
    chibiSleepRadio(1);               // sleep the radio
#endif

    // initialize all global variables
    // set initial states of GPS, log file, Geiger counter, etc.
    global_variables_init();

    // run diagnostic (except when waking for SD reader)
    //if (!sd_reader_interrupted)
    //diagnostics();

    // ***WARNING*** turn High Voltage board ON ***WARNING***
    bg_hvps_on();
    delay(10); // wait for power to stabilize

    // And now Start the Pulse Counter!
    hwc.start();

    // Starting now!
    Serial.println("bGeigie powered on!");

#if SD_READER_ENABLE
  }
#endif

}

void shutdown()
{
  // indicate something is happening
  blinky(BLINK_OFF);

  bg_gps_off();
  chibiSleepRadio(1);
  bg_hvps_off();
  bg_sensors_off();
  bg_led_off();

  // say goodbye...
  Serial.println("bGeigie sleeps... good night.");
  delay(10);
  Serial.flush();
}

void check_battery_voltage()
{
  
  battery_voltage = 1000*bgs_read_battery(); // mV
  if (battery_voltage < BATT_SHUTDOWN_VOLTAGE)
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

  if (theConfig.hv_sense)
    strcpy_P(tmp, PSTR("# HV sense enabled,yes"));
  else
    strcpy_P(tmp, PSTR("# HV sense enabled,no"));
  sd_log_writeln(filename, tmp);

  if (theConfig.coord_truncation)
  {
    strcpy_P(tmp, PSTR("# Coordinate truncation enabled,yes"));
    sd_log_writeln(filename, tmp);
  }
  else
  {
    strcpy_P(tmp, PSTR("# Coordinate truncation enabled,no"));
    sd_log_writeln(filename, tmp);
  }

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
  Serial.println(theConfig.id, HEX);

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
  if (theConfig.hv_sense)
  {
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
  }
  else
  {
    strcpy_P(tmp, PSTR("HV sense enabled,no"));
    Serial.println(tmp);
  }

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

  if (theConfig.coord_truncation)
  {
    strcpy_P(tmp, PSTR("Coordinate truncation enabled,yes"));
    Serial.println(tmp);
  } 
  else 
  {
    strcpy_P(tmp, PSTR("Coordinate truncation enabled,no"));
    Serial.println(tmp);
  }

  strcpy_P(tmp, PSTR("--- Diagnostic END ---"));
  Serial.println(tmp);
}


/**********************/
/* JP truncation code */
/**********************/

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

