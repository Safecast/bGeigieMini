/*
   The bGeigie-mini
   A device for car-borne radiation measurement (aka Radiation War-driving).

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

#include <SD.h>
#include <SPI.h>
#include <chibi.h>
#include <limits.h>
#include <EEPROM.h>
#include <GPS.h>
#include <InterruptCounter.h>
#include <math.h>
#include <avr/wdt.h>

#include <sd_logger.h>

#define TIME_INTERVAL 5000
#define NX 12
#define LED_ENABLED 0
#define PRINT_BUFSZ 80
#define AVAILABLE 'A'          // indicates geiger data are ready (available)
#define VOID      'V'          // indicates geiger data not ready (void)
#define BMRDD_EEPROM_ID 100
#define BMRDD_ID_LEN 3

// radio options
#define CHANNEL 20
#define DEST_ADDR 0xFFFF      // this is the 802.15.4 broadcast address
#define RX_ADDR_BASE 0x2000   // base for radio address. Add device number to make complete address

// Supply voltage control
// does not write to SD card when supply voltage
// is lower than this value (in mV).
#define VCC_LOW_LIMIT 4750

// compile time options
#define DIAGNOSTIC_ENABLE 1
#define VOLTAGE_SENSE_ENABLE 0
#define PLUSSHIELD 0
#define JAPAN_POST 0
#define RADIO_ENABLE 1
#define GPS_PROGRAMMING 1

// GPS type
#define GPS_MTK 1
#define GPS_CANMORE 2
#define GPS_TYPE GPS_MTK

// make sure to include that after JAPAN_POST is defined
#include "version.h"

static const int chipSelect = 10;
static const int radioSelect = A3;
static const int sdPwr = 4;

// SD card detect and write protect input pins
static const int sd_cd = 6;
static const int sd_wp = 7;

#if VOLTAGE_SENSE_ENABLE
// We read voltage on analog pins 0 and 1
static const int pinV0 = A0;
static const int pinV1 = A1;
#endif

#if PLUSSHIELD
// pin to enable boost converter of LiPo battery
static const int pinBoost = 2;
#endif

unsigned long shift_reg[NX] = {0};
unsigned long reg_index = 0;
unsigned long total_count = 0;
int str_count = 0;
char geiger_status = VOID;

// the line buffer for serial receive and send
static char line[LINE_SZ];

char filename[13];      // placeholder for filename
char hdr[6] = "BMRDD";  // header for sentence
char hdr_status[6] = "BMSTS";  // Status message header
char dev_id[BMRDD_ID_LEN+1];  // device id
char ext_log[] = ".log";
char ext_bak[] = ".sts";

#if RADIO_ENABLE
uint8_t radio_init_status = 0;
#endif

// RTC acquisition state variable.
int rtc_acq = 0;
// This is to compare the year we get from the GPS
// and determine if at least the time has been acquired.
// We do that by comparing to the year default value
// that is hardcoded in the GPS module. This value is
// model dependent. So far, it works only for the MTK3339
// that actually do have an RTC onboard.
#if GPS_TYPE == GPS_MTK
  static char *rtc_year_cmp = "80";
#endif

/**************************************************************************/
/*!

*/
/**************************************************************************/
void setup()
{
  char tmp[25];

  // init serial
  Serial.begin(9600);

  // enable and reset the watchdog timer
  wdt_enable(WDTO_8S);    // the arduino will automatically reset if it hangs for more than 8s
  wdt_reset();
#if TIME_INTERVAL > 7000
  Serial.println("WARNING: LOOP INTERVAL CLOSE TO WATCHDOG TIMOUT");
#endif
  
  // initialize and program the GPS module
#if GPS_PROGRAMMING
  gps_program_settings();
#endif

  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode(chipSelect, OUTPUT);
  digitalWrite(chipSelect, HIGH);     // disable SD card chip select 
  
  pinMode(radioSelect, OUTPUT);
  digitalWrite(radioSelect, HIGH);    // disable radio chip select
  
  pinMode(sdPwr, OUTPUT);
  digitalWrite(sdPwr, LOW);           // turn on SD card power

#if VOLTAGE_SENSE_ENABLE
  // setup analog reference to read battery and boost voltage
  analogReference(INTERNAL);
#endif

#if PLUSSHIELD
  // setup pin for boost converter
  pinMode(pinBoost, OUTPUT);
  digitalWrite(pinBoost, LOW);
  delay(20);
  pinMode(pinBoost, INPUT);
#endif
  
  // Create pulse counter on INT1
  interruptCounterSetup(1, TIME_INTERVAL);

  // init GPS using default Serial connection
  gps_init(&Serial, line);
  
  // set device id
  pullDevId();

#if RADIO_ENABLE
  // init chibi on channel normally 20
  radio_init_status = chibiInit();
  chibiSetChannel(CHANNEL);
  unsigned int addr = getRadioAddr();
  chibiSetShortAddr(addr);
  chibiSleepRadio(1);
#endif

  // initialize SD card
  sd_log_init(sdPwr, sd_cd, chipSelect);

  // set initial state of Geiger to void
  geiger_status = VOID;
  str_count = 0;

  // Run the diagnostic routine
  diagnostics();
  
  // And now Start the Pulse Counter!
  interruptCounterReset();

  // Starting now!
  Serial.println("Starting now!");
}

/**************************************************************************/
/*!

*/
/**************************************************************************/
void loop()
{
  char tmp[25];

  // update gps
  gps_update();

  // generate CPM every TIME_INTERVAL seconds
  if (interruptCounterAvailable())
  {
#if PLUSSHIELD
    // give a pulse to enable boost converter of LiPo pack
    pinMode(pinBoost, OUTPUT);
    digitalWrite(pinBoost, LOW);
    delay(20);
    pinMode(pinBoost, INPUT);
#endif
        
    // first, reset the watchdog timer
    wdt_reset();

    if (gps_available())
    {
      unsigned long cpm=0, cpb=0;
      byte line_len;

      // obtain the count in the last bin
      cpb = interruptCounterCount();

      // reset the pulse counter
      interruptCounterReset();

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
      
      // generate timestamp. only update the start time if 
      // we printed the timestamp. otherwise, the GPS is still 
      // updating so wait until its finished and generate timestamp
      memset(line, 0, LINE_SZ);

      // generate log sentence
      line_len = gps_gen_timestamp(line, shift_reg[reg_index], cpm, cpb);

#if RADIO_ENABLE
      // send out wirelessly. first wake up the radio, do the transmit, then go back to sleep
      if (radio_init_status)
      {
        chibiSleepRadio(0);
        delay(10);
        chibiTx(DEST_ADDR, (byte *)line, LINE_SZ);
        chibiSleepRadio(1);
      }
#endif

#if VOLTAGE_SENSE_ENABLE
      // check the supply voltage before writing to SD card
      int vcc = (int)(1000*read_voltage(pinV1));
#endif

#if GPS_TYPE == GPS_MTK
      if (rtc_acq == 0 && ( gps_getData()->status[0] == AVAILABLE 
          || strncmp(gps_getData()->datetime.year, rtc_year_cmp, 2) != 0) )
#else
      if (rtc_acq == 0 && gps_getData()->status[0] == AVAILABLE)
#endif
      {
        // flag GPS acquired
        rtc_acq = 1;

#if VOLTAGE_SENSE_ENABLE
        if (vcc > VCC_LOW_LIMIT)  // beginning of low VCC if statement
        {
#endif
          // Create the filename for that drive
          strcpy(filename, dev_id);
          strcat(filename, "-");
          strncat(filename, gps_getData()->date+2, 2);
          strncat(filename, gps_getData()->date, 2);
          strcpy(filename+8, ext_log);

          // write the header and options to the new file
          sd_log_init(sdPwr, sd_cd, chipSelect);
          writeHeader2SD(filename);

#if DIAGNOSTIC_ENABLE
          // write to backup file as well
          strcpy(filename+8, ext_bak);

          // write the header and options to the new file
          writeHeader2SD(filename);
#endif

#if VOLTAGE_SENSE_ENABLE
        } // end of low VCC if statement
#endif
      }
      
      // Printout line
      if (rtc_acq == 0)
        Serial.print("No RTC: "); // add foot note here :)
      Serial.println(line);

      // Once the time has been acquired, save to file
#if VOLTAGE_SENSE_ENABLE
      if (rtc_acq != 0 && vcc > VCC_LOW_LIMIT)
#else
      if (rtc_acq != 0)
#endif
      {
        // dump data to SD card
        sd_log_init(sdPwr, sd_cd, chipSelect);
        strcpy(filename+8, ext_log);
        sd_log_writeln(filename, line);

      }

#if DIAGNOSTIC_ENABLE
      // Generate a diagnostic sentence
      bg_status_str_gen(line);
      Serial.println(line);

#if VOLTAGE_SENSE_ENABLE
      if (rtc_acq != 0 && vcc > VCC_LOW_LIMIT)
#else
      if (rtc_acq != 0)
#endif
      {
        // write to backup file as well
        sd_log_init(sdPwr, sd_cd, chipSelect);
        strcpy(filename+8, ext_bak);
        sd_log_writeln(filename, line);
      }
#endif // DIAGNOSTIC_ENABLE

#if RADIO_ENABLE
      // send out wirelessly the diagnostic sentence
      if (radio_init_status)
      {
        chibiSleepRadio(0);
        delay(10);
        chibiTx(DEST_ADDR, (byte *)line, LINE_SZ);
        chibiSleepRadio(1);
      }
#endif

    }

  }
}

/**************************************************************************/
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
  sprintf_P(buf, PSTR("$%s,%s,20%s-%s-%sT%s:%s:%sZ,%ld,%ld,%ld,%c,%s,%s,%s,%s,%s,%s,%s,%s"),  \
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

#if DIAGNOSTIC_ENABLE
/* create Status log line */
byte bg_status_str_gen(char *buf)
{
  byte len;
  byte chk;

  // get GPS data
  gps_t *ptr = gps_getData();

  // create string
  memset(buf, 0, LINE_SZ);

#if VOLTAGE_SENSE_ENABLE
  int batt = (int)1000*read_voltage(pinV0); // battery voltage [mV]
  int vcc = (int)1000*read_voltage(pinV1);  // supply voltage  [mV]
  sprintf_P(buf, PSTR("$%s,%s,20%s-%s-%sT%s:%s:%sZ,%s,%s,%s,%s,v%s,%d,%d,%d,%d,%d"),  \
              hdr_status, \
              dev_id, \
              ptr->datetime.year, ptr->datetime.month, ptr->datetime.day,  \
              ptr->datetime.hour, ptr->datetime.minute, ptr->datetime.second, \
              ptr->lat, ptr->lat_hem, \
              ptr->lon, ptr->lon_hem, \
              version, \
              batt, \
              vcc, \
              sd_log_inserted, sd_log_initialized, sd_log_last_write);
#else
  sprintf_P(buf, PSTR("$%s,%s,20%s-%s-%sT%s:%s:%sZ,%s,%s,%s,%s,v%s,,,%d,%d,%d"),  \
              hdr_status, \
              dev_id, \
              ptr->datetime.year, ptr->datetime.month, ptr->datetime.day,  \
              ptr->datetime.hour, ptr->datetime.minute, ptr->datetime.second, \
              ptr->lat, ptr->lat_hem, \
              ptr->lon, ptr->lon_hem, \
              version, \
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
#endif


/**************************************************************************/
/*!

*/
/**************************************************************************/
unsigned long cpm_gen()
{
   unsigned int i;
   unsigned long c_p_m = 0;
   
   // sum up
   for (i=0 ; i < NX ; i++)
     c_p_m += shift_reg[i];
   
   return c_p_m;
}

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

/*
 * Set the address for the radio based on device ID
 */
unsigned int getRadioAddr()
{
  // prefix with two so that it never collides with broadcast address
  unsigned int addr = RX_ADDR_BASE;

  // first digit
  if (dev_id[0] != 'X')
    addr += (unsigned int)(dev_id[0]-'0') * 0x100;

  // second digit
  if (dev_id[1] != 'X')
    addr += (unsigned int)(dev_id[1]-'0') * 0x10;

  // third digit
  if (dev_id[2] != 'X')
    addr += (unsigned int)(dev_id[2]-'0') * 0x1;

  return addr;
}

/*************************/
/* Write Options to File */
/*************************/

void writeHeader2SD(char *filename)
{
  char tmp[50];

  sd_log_init(sdPwr, sd_cd, chipSelect);

  strcpy_P(tmp, PSTR("# Welcome to Safecast bGeigie-Mini System"));
  sd_log_writeln(filename, tmp);

  strcpy_P(tmp, PSTR("# Version,"));
  strcat(tmp, version);
  sd_log_writeln(filename, tmp);

#if GPS_TYPE == GPS_CANMORE
  strcpy_P(tmp, PSTR("# GPS type,Canmore"));
#elif GPS_TYPE == GPS_MTK
  strcpy_P(tmp, PSTR("# GPS type,MTK"));
#else
  strcpy_P(tmp, PSTR("# GPS type,unknown"));
#endif
  sd_log_writeln(filename, tmp);

#if RADIO_ENABLE
  strcpy_P(tmp, PSTR("# Radio enabled,yes"));
#else
  strcpy_P(tmp, PSTR("# Radio enabled,no"));
#endif
  sd_log_writeln(filename, tmp);

#if GPS_PROGRAMMING
  strcpy_P(tmp, PSTR("# GPS programming,yes"));
#else
  strcpy_P(tmp, PSTR("# GPS programming,no"));
#endif
  sd_log_writeln(filename, tmp);
  
#if DIAGNOSTIC_ENABLE
  strcpy_P(tmp, PSTR("# Diagnostic file,yes"));
  sd_log_writeln(filename, tmp);
#else
  strcpy_P(tmp, PSTR("# Diagnostic file,no"));
  sd_log_writeln(filename, tmp);
#endif

#if VOLTAGE_SENSE_ENABLE
  strcpy_P(tmp, PSTR("# Voltage sense,yes"));
  sd_log_writeln(filename, tmp);
  // battery voltage
  int v0 = (int)(1000*read_voltage(pinV0));
  v0 = (int)(1000*read_voltage(pinV0)); // read 2nd time for stable value
  sprintf_P(tmp, PSTR("# Battery voltage,%dmV"), v0);
  sd_log_writeln(filename, tmp);

  // boost voltage
  int v1 = (int)(1000*read_voltage(pinV1));
  v1 = (int)(1000*read_voltage(pinV1)); // read 2nd time for stable value
  sprintf_P(tmp, PSTR("# Supply voltage,%dmV"), v0);
  sd_log_writeln(filename, tmp);
#else
  strcpy_P(tmp, PSTR("# Voltage sense,no"));
  sd_log_writeln(filename, tmp);
#endif

  // System free RAM
  sprintf_P(tmp, PSTR("# System free RAM,%dB"), FreeRam());
  sd_log_writeln(filename, tmp);

#if JAPAN_POST
  strcpy_P(tmp, PSTR("# Coordinate truncation enabled,yes"));
  sd_log_writeln(filename, tmp);
#else
  strcpy_P(tmp, PSTR("# Coordinate truncation enabled,no"));
  sd_log_writeln(filename, tmp);
#endif

#if PLUSSHIELD
  strcpy_P(tmp, PSTR("# PlusShield,yes"));
  sd_log_writeln(filename, tmp);
#else
  strcpy_P(tmp, PSTR("# PlusShield,no"));
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

  strcpy_P(tmp, PSTR("bGeigie-mini System"));
  Serial.println(tmp);

  // Version number
  strcpy_P(tmp, PSTR("Version,"));
  Serial.print(tmp);
  Serial.println(version);

  // Device ID
  strcpy_P(tmp, PSTR("Device ID,"));
  Serial.print(tmp);
  Serial.println(dev_id);

  // SD card
  sd_log_card_diagnostic();  

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

#if GPS_TYPE == GPS_CANMORE
  strcpy_P(tmp, PSTR("GPS type,Canmore"));
#elif GPS_TYPE == GPS_MTK
  strcpy_P(tmp, PSTR("GPS type,MTK"));
#else
  strcpy_P(tmp, PSTR("GPS type,unknown"));
#endif
  Serial.println(tmp);

  strcpy_P(tmp, PSTR("GPS programming,"));
  Serial.print(tmp);
#if GPS_PROGRAMMING
  strcpy_P(tmp, PSTR("yes"));
#else
  strcpy_P(tmp, PSTR("no"));
#endif
  Serial.println(tmp);

#if DIAGNOSTIC_ENABLE
  strcpy_P(tmp, PSTR("Diagnostic file,yes"));
#else
  strcpy_P(tmp, PSTR("Diagnostic file,no"));
#endif
  Serial.println(tmp);

#if VOLTAGE_SENSE_ENABLE
  strcpy_P(tmp, PSTR("Voltage sense,yes"));
  Serial.println(tmp);

  // battery voltage
  int v0 = (int)(1000*read_voltage(pinV0));
  v0 = (int)(1000*read_voltage(pinV0)); // read 2nd time for stable value
  strcpy_P(tmp, PSTR("Battery voltage,"));
  Serial.print(tmp);
  Serial.print(v0);
  strcpy_P(tmp, PSTR("mV"));
  Serial.println(tmp);

  // boost voltage
  int v1 = (int)(1000*read_voltage(pinV1));
  v1 = (int)(1000*read_voltage(pinV1)); // read 2nd time for stable value
  strcpy_P(tmp, PSTR("Supply voltage,"));
  Serial.print(tmp);
  Serial.print(v1);
  strcpy_P(tmp, PSTR("mV"));
  Serial.println(tmp);
#else
  strcpy_P(tmp, PSTR("Voltage sense,no"));
  Serial.println(tmp);
#endif

  // System free RAM
  strcpy_P(tmp, PSTR("System free RAM,"));
  Serial.print(tmp);
  Serial.print(FreeRam());
  Serial.println('B');

#if JAPAN_POST
  strcpy_P(tmp, PSTR("Coordinate truncation enabled,yes"));
#else
  strcpy_P(tmp, PSTR("Coordinate truncation enabled,no"));
#endif
  Serial.println(tmp);

#if PLUSSHIELD
  strcpy_P(tmp, PSTR("PlusShield,yes"));
#else
  strcpy_P(tmp, PSTR("PlusShield,no"));
#endif
  Serial.println(tmp);
  
  strcpy_P(tmp, PSTR("--- Diagnostic END ---"));
  Serial.println(tmp);
}


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


#if DIAGNOSTIC_ENABLE
float read_voltage(int pin)
{
  return 1.1*analogRead(pin)/1023. * 10.1;
}
#endif

#if GPS_PROGRAMMING
void gps_program_settings()
{
  // wait for GPS to start
  int t = 0;
  while(!Serial.available() && t < 5000)
  {
    delay(10);
    t += 10;
  }

#if GPS_TYPE == GPS_CANMORE

  // all GPS command taken from datasheet
  // "Binary Messages Of SkyTraq Venus 6 GPS Receiver"

  // set GGA and RMC output at 1Hz
  uint8_t GPS_MSG_OUTPUT_GGARMC_1S[9] = { 0x08, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01 }; // with update to RAM and FLASH
  uint16_t GPS_MSG_OUTPUT_GGARMC_1S_L = 9;

  // Power Save mode (not sure what it is doing at the moment
  uint8_t GPS_MSG_PWR_SAVE[3] = { 0x0C, 0x01, 0x01 }; // update to FLASH too
  uint16_t GPS_MSG_PWR_SAVE_L = 3;

  // send all commands
  gps_send_message(GPS_MSG_OUTPUT_GGARMC_1S, GPS_MSG_OUTPUT_GGARMC_1S_L);
  gps_send_message(GPS_MSG_PWR_SAVE, GPS_MSG_PWR_SAVE_L);

#elif GPS_TYPE == GPS_MTK

  char tmp[55];

  /* Set GPS output to NMEA GGA and RMC only at 1Hz */
  strcpy_P(tmp, PSTR(MTK_SET_NMEA_OUTPUT_RMCGGA));
  Serial.println(tmp);
  strcpy_P(tmp, PSTR(MTK_UPDATE_RATE_1HZ));
  Serial.println(tmp);

#endif /* GPS_TYPE */
}
#endif /* GPS_PROGAMMING */

#if GPS_PROGRAMMING && GPS_TYPE == GPS_CANMORE
void gps_send_message(const uint8_t *msg, uint16_t len)
{
  uint8_t chk = 0x0;

#if ARDUINO < 100
  // header
  Serial.print(0xA0, BYTE);
  Serial.print(0xA1, BYTE);
  // send length
  Serial.print(len >> 8, BYTE);
  Serial.print(len & 0xff, BYTE);
  // send message
  for (int i = 0 ; i < len ; i++)
  {
    Serial.print(msg[i], BYTE);
    chk ^= msg[i];
  }
  // checksum
  Serial.print(chk, BYTE);
  // end of message
  Serial.print(0x0D, BYTE);
  Serial.println(0x0A, BYTE);
#else
  // header
  Serial.write(0xA0);
  Serial.write(0xA1);
  // send length
  Serial.write(len >> 8);
  Serial.write(len & 0xff);
  // send message
  for (int i = 0 ; i < len ; i++)
  {
    Serial.write(msg[i]);
    chk ^= msg[i];
  }
  // checksum
  Serial.write(chk);
  // end of message
  Serial.write(0x0D);
  Serial.write(0x0A);
  Serial.write((byte)'\r');
  Serial.write((byte)'\n');
#endif
}
#endif /* GPS_CANMORE */
