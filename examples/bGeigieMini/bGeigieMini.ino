#include <SD.h>
#include <chibi.h>
#include <limits.h>
#include <EEPROM.h>
#include "gps_geiger.h"

#define TIME_INTERVAL 5000
#define NX 12
#define DEST_ADDR 0x1234
#define CHANNEL 20
#define TX_ENABLED 1
#define LED_ENABLED 0
#define PRINT_BUFSZ 80
#define AVAILABLE 'A'          // indicates geiger data are ready (available)
#define VOID      'V'          // indicates geiger data not ready (void)
#define BMRDD_EEPROM_ID 100
#define BMRDD_ID_LEN 3

static const int chipSelect = 10;
static const int radioSelect = A3;
static const int sdPwr = 4;

#if LED_ENABLED
static const int greenLED = 8;
static const int redLED = 9;
#endif

static char line[LINE_SZ];
static byte i = 0;
static gps_t gps;
static unsigned long start_time;

unsigned long shift_reg[NX] = {0};
unsigned long reg_index = 0;
unsigned long total_count = 0;
int str_count = 0;
char geiger_status = VOID;

// This is the data file object that we'll use to access the data file
File dataFile; 

static char msg1[] PROGMEM = "SD init...\n";
static char msg2[] PROGMEM = "Card failure...\n";
static char msg3[] PROGMEM = "Card initialized\n";
static char msg4[] PROGMEM = "Error: Log file cannot be opened.\n";
static char msg5[] PROGMEM = "Device Id: ";

char filename[13];      // placeholder for filename
char hdr[6] = "BMRDD";  // header for sentence
char dev_id[BMRDD_ID_LEN+1];  // device id
char ext_log[] = ".log";
char ext_bak[] = ".bak";
char fileHeader[] = "# NEW LOG\n# format=1.1.1\n";
 
// State variables
int gps_init_acq = 0;
int log_created = 0;

/**************************************************************************/
/*!

*/
/**************************************************************************/
void setup()
{
  char tmp[25];
  gps_init();
  
  // init serial
  Serial.begin(9600);

  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode(chipSelect, OUTPUT);
  digitalWrite(chipSelect, HIGH);     // disable SD card chip select 
  
  pinMode(radioSelect, OUTPUT);
  digitalWrite(radioSelect, HIGH);    // disable radio chip select
  
  pinMode(sdPwr, OUTPUT);
  digitalWrite(sdPwr, LOW);           // turn on SD card power
  
  // attach interrupt to INT1
  attachInterrupt(1, count_pulse, RISING);

  
  // set device id
  pullDevId();

  strcpy_P(tmp, msg5);
  Serial.print(tmp);
  Serial.println(dev_id);

  // print free RAM. we should try to maintain about 300 bytes for stack space
  Serial.println(FreeRam());

#if TX_ENABLED
  // init chibi on channel normally 20
  chibiInit();
  chibiSetChannel(CHANNEL);
#endif

  // turn SD card on
  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);
  
  strcpy_P(tmp, msg1);
  Serial.print(tmp);
  
  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    // don't do anything more:
    int printOnce = 1;
    while(1) 
    {
      if (printOnce) {
        printOnce = 0;
        strcpy_P(tmp, msg2);
        Serial.print(tmp);
      }
    }
  }
  strcpy_P(tmp, msg3);
  Serial.print(tmp);

  // set initial state of Geiger to void
  geiger_status = VOID;
  str_count = 0;
  
  // print free RAM. we should try to maintain about 300 bytes for stack space
  Serial.println(FreeRam());

  start_time = millis();
  //digitalWrite(sdPwr, HIGH);

#if LED_ENABLED
  // initialize LEDs
  pinMode(redLED, OUTPUT);
  digitalWrite(redLED, LOW);
  pinMode(greenLED, OUTPUT);
  digitalWrite(greenLED, HIGH);
#endif

}

/**************************************************************************/
/*!

*/
/**************************************************************************/
void loop()
{
  char tmp[25];
  char *tok[SYM_SZ] = {0};
  int j = 0;

  while (Serial.available())
  {
    if (i < LINE_SZ)
    {
      line[i] = Serial.read();
      if (line[i] == '\n')
      {
        line[i+1] = '\0';
        i=0;

        // dump the raw GPS data
        //Serial.print(line);

        // tokenize line
        tok[j] = strtok(line, ",");
        do
        {
           tok[++j] = strtok(NULL, ",");
        } while ((j < SYM_SZ) && (tok[j] != NULL));

        // reset the index variable for next run
        j = 0;

        // parse line and date/time and update gps struct
        if (strcmp(tok[0], "$GPRMC") == 0)
        {
          parse_line_rmc(tok);
          parse_datetime();
        }
        else if (strcmp(tok[0], "$GPGGA") == 0)
        {
           parse_line_gga(tok);
        }
        
        // clear the flag so that we know its okay to read the data
        gps.updating = 0;
      }
      else
      {
        // still taking in data. increment index and make sure the updating flag is set
        gps.updating = 1;
        i++;
      }
    }
    else
    {
      i = 0;
    }
  }
  
  // generate CPM every TIME_INTERVAL seconds
  if (elapsed_time(start_time) > TIME_INTERVAL)
  {
    if (!gps.updating)
    {
      unsigned long cpm=0, cpb=0;
      byte line_len;

      noInterrupts();                 // XXX stop interrupts here
      cpm = cpm_gen();                // compute last minute counts
      cpb = shift_reg[reg_index];     // get current bin count
      reg_index = (reg_index+1) % NX; // increment register index
      shift_reg[reg_index] = 0;       // reset new register
      interrupts();                   // XXX Resume interrupts here

      // update the start time for the next interval
      start_time = millis();

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

      line_len = gps_gen_timestamp(line, shift_reg[reg_index], cpm, cpb);

#if LED_ENABLED
      // check GPS status and blink LED accordingly
      if (gps.status[0] == AVAILABLE && geiger_status == AVAILABLE)
        blink(greenLED, 4, 100, 100);
      else
        blink(redLED, 4, 100, 100);
#endif

      if (gps_init_acq == 0 && gps.status[0] == AVAILABLE)
      {
        // flag GPS acquired
        gps_init_acq = 1;
        // Create the filename for that drive
        strcpy(filename, dev_id);
        strcat(filename, "-");
        strncat(filename, gps.date+2, 2);
        strncat(filename, gps.date, 2);
        // print some comment line to mark beginning of new log
        strcpy(filename+8, ext_log);
        dataFile = SD.open(filename, FILE_WRITE);
        if (dataFile)
        {
          Serial.print("Filename: ");
          Serial.println(filename);
          dataFile.print(fileHeader);
          dataFile.close();
        }
        else
        {
          char tmp[40];
          strcpy_P(tmp, msg4);
          Serial.print(tmp);
        }
        // write to backup file as well
        strcpy(filename+8, ext_bak);
        dataFile = SD.open(filename, FILE_WRITE);
        if (dataFile)
        {
          dataFile.print(fileHeader);
          dataFile.close();
        }
        else
        {
          char tmp[40];
          strcpy_P(tmp, msg4);
          Serial.print(tmp);
        }
      }
      
      // turn on SD card power and delay a bit to initialize
      //digitalWrite(sdPwr, LOW);
      //delay(10);
      
      // init the SD card see if the card is present and can be initialized:
      //while (!SD.begin(chipSelect));

      if (gps_init_acq == 0)
      {
        Serial.print("No GPS: ");
        Serial.println(line);
#if TX_ENABLED
        // send out wirelessly. first wake up the radio, do the transmit, then go back to sleep
        chibiSleepRadio(0);
        delay(10);
        chibiTx(DEST_ADDR, (byte *)line, LINE_SZ);
        chibiSleepRadio(1);
#endif
      }
      else
      {
        // dump data to SD card
        strcpy(filename+8, ext_log);
        dataFile = SD.open(filename, FILE_WRITE);
        if (dataFile)
        {
          Serial.println(line);
          dataFile.print(line);
          dataFile.print("\n");
          dataFile.close();

  #if TX_ENABLED
          // send out wirelessly. first wake up the radio, do the transmit, then go back to sleep
          chibiSleepRadio(0);
          delay(10);
          chibiTx(DEST_ADDR, (byte *)line, LINE_SZ);
          chibiSleepRadio(1);
  #endif
        }
        else
        {
          char tmp[40];
          strcpy_P(tmp, msg4);
          Serial.print(tmp);
        }   
        
        // write to backup file as well
        strcpy(filename+8, ext_bak);
        dataFile = SD.open(filename, FILE_WRITE);
        if (dataFile)
        {
          dataFile.print(line);
          dataFile.print("\n");
          dataFile.close();
        }
        else
        {
          char tmp[40];
          strcpy_P(tmp, msg4);
          Serial.print(tmp);
        }
        
        //turn off sd power        
        //digitalWrite(sdPwr, HIGH); 
      }
    }
  }
}

/**************************************************************************/
/*!

*/
/**************************************************************************/
static void gps_init()
{
  memset(&gps, 0, sizeof(gps_t));
  
  // set static values first
  strcpy(gps.dev_name, "GT-703");
  strcpy(gps.meas_type, "AIR");
}

/**************************************************************************/
/*!

*/
/**************************************************************************/
static void parse_line_rmc(char **token)
{
  memcpy(&gps.utc,        token[1],     UTC_SZ-1);
  memcpy(&gps.status,     token[2],     DEFAULT_SZ-1);
  memcpy(&gps.lat,        token[3],     LAT_SZ-1);
  memcpy(&gps.lat_hem,    token[4],     DEFAULT_SZ-1);
  memcpy(&gps.lon,        token[5],     LON_SZ-1);
  memcpy(&gps.lon_hem,    token[6],     DEFAULT_SZ-1);
  memcpy(&gps.speed,      token[7],     SPD_SZ-1);
  memcpy(&gps.course,     token[8],     CRS_SZ-1);
  memcpy(&gps.date,       token[9],     DATE_SZ-1);
  memcpy(&gps.checksum,   token[10],    CKSUM_SZ-1);
}

/**************************************************************************/
/*!

*/
/**************************************************************************/
void parse_line_gga(char **token)
{
    memcpy(&gps.quality,    token[6], DEFAULT_SZ-1);
    memcpy(&gps.num_sat,   token[7], NUM_SAT_SZ-1);
    memcpy(&gps.precision, token[8], PRECISION_SZ-1);
    memcpy(&gps.altitude,  token[9], ALTITUDE_SZ-1);
}


/**************************************************************************/
/*!

*/
/**************************************************************************/
void parse_datetime()
{
    memset(&gps.datetime, 0, sizeof(date_time_t));

    // parse UTC time
    memcpy(gps.datetime.hour, &gps.utc[0], 2);
    memcpy(gps.datetime.minute, &gps.utc[2], 2);
    memcpy(gps.datetime.second, &gps.utc[4], 2);

    // parse UTC calendar
    memcpy(gps.datetime.day, &gps.date[0], 2);
    memcpy(gps.datetime.month, &gps.date[2], 2);
    memcpy(gps.datetime.year, &gps.date[4], 2);
}

/**************************************************************************/
/*!

*/
/**************************************************************************/
byte gps_gen_timestamp(char *buf, unsigned long counts, unsigned long cpm, unsigned long cpb)
{
  byte len;
  byte chk;
  
  memset(buf, 0, LINE_SZ);
  sprintf(buf, "$%s,%s,20%s-%s-%sT%s:%s:%sZ,%ld,%ld,%ld,%c,%s,%s,%s,%s,%s,%s,%s,%s",  \
              hdr, \
              dev_id, \
              gps.datetime.year, gps.datetime.month, gps.datetime.day,  \
              gps.datetime.hour, gps.datetime.minute, gps.datetime.second, \
              cpm, \
              cpb, \
              total_count, \
              geiger_status, \
              gps.lat, gps.lat_hem, \
              gps.lon, gps.lon_hem, \
              gps.altitude, \
              gps.status, \
              gps.precision, \
              gps.quality);
   len = strlen(buf);
   buf[len] = '\0';

   // generate checksum
   chk = checksum(buf+1, len);

   // add checksum to end of line before sending
   if (chk < 16)
     sprintf(buf + len, "*0%X", (int)chk);
   else
     sprintf(buf + len, "*%X", (int)chk);
      

   return len;
}

/**************************************************************************/
// calculate elapsed time
/**************************************************************************/
unsigned long elapsed_time(unsigned long start_time)
{
  unsigned long stop_time = millis();
  
  if (start_time >= stop_time)
  {
    return start_time - stop_time;
  }
  else
  {
    return (ULONG_MAX - (start_time - stop_time));
  }
}

/**************************************************************************/
/*!

*/
/**************************************************************************/
void count_pulse()
{
  shift_reg[reg_index]++;
}

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

/**************************************************************************/
/*!
compute check sum of N bytes in array s
*/
/**************************************************************************/
char checksum(char *s, int N)
{
  int i = 0;
  char chk = s[0];

  for (i=1 ; i < N ; i++)
    chk ^= s[i];

  return chk;
}

void pullDevId()
{
  for (int i=0 ; i < BMRDD_ID_LEN ; i++)
  {
    dev_id[i] = (char)EEPROM.read(i+BMRDD_EEPROM_ID);
    if (dev_id[i] < '0' || dev_id[i] > '9')
    {
      dev_id[i] = '0';
      EEPROM.write(i+BMRDD_EEPROM_ID, dev_id[i]);
    }
  }
  dev_id[BMRDD_ID_LEN] = NULL;
}

#if LED_ENABLED
void blink(int LED, int p, int t_up, int t_dw)
{
  if (p == 0)
    return;

  while (p > 0)
  {
    digitalWrite(LED, HIGH);
    delay(t_up);
    digitalWrite(LED, LOW);
    p--;
    if (p > 0)
      delay(t_dw);
  }
}
#endif

