/*
   The bGeigie-Ninja
   This is the firmware for the remote display monitor for the bGeigie-mini.

   Copyright (c) 2011, Robin Scheibler aka FakuFaku
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

#include "bGeigieNinja2.h"

#include <Wire.h>

#include <chibi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#include "version.h"
#include "pwr.h"

/* device id length */
#define BMRDD_ID_LEN 3

#define MAX_PKT_SIZE 127

#define DIM_TIME 60000
#define DIM_LEN 1000

#define RX_ADDR 0x1234
#define CHANNEL 20

#define STRBUFFER_SZ 32
#define SYM_SZ 32

// a buffer line for the oled display
static char strbuffer[STRBUFFER_SZ];

// initialize the library with the numbers of the interface pins
int OLED_PWR    = 5;
int OLED_CLK    = 12;
int OLED_CS     = 10;
int OLED_RESET  = 9;
int OLED_DC     = 8;
int OLED_DATA   = 4;
Adafruit_SSD1306 display(OLED_DATA, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

// pin layout
int buzzerPin = 3;
int pinSwitch = 2;
int led = 13;

// link status variable
char data_corrupt_flag = 0;

// Array of connected devices devices
BGeigieDev devices[MAX_DEV_NUM];
uint8_t dev_index = 0;  // current device index


/**************************************************************************/
// Initialize
/**************************************************************************/
void setup()
{  
  char tmp[40];

  // this is needed to ensure the uC is always a master on SPI
  pinMode(10, OUTPUT);

  // set pins
  pinMode(buzzerPin, OUTPUT);
  pinMode(led, OUTPUT);

  // setup display power
  pinMode(OLED_PWR, OUTPUT);
  digitalWrite(OLED_PWR, LOW);
  
  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC);
  //display.setRotation(2); // because the oled screen is upside down.
  // init done

  display.display(); // show splashscreen
  delay(1000);

  // Initialize the chibi command line and set the speed to 9600 bps
  //while (!Serial);        // uncomment this when debugging with serial
  Serial.begin(57600);
  

  strcpy_P(tmp, PSTR("Safecast bGeigie Ninja, version ")); 
  Serial.print(tmp);
  Serial.println(version);

  // Initialize the chibi wireless stack
  chibiInit();
  
  Serial.println(FreeRam());

  strcpy_P(tmp, PSTR("Init chibi done."));
  Serial.println(tmp);

  // set address
  chibiSetShortAddr(RX_ADDR);
  strcpy_P(tmp, PSTR("Just set address to 0x"));
  Serial.print(tmp);
  Serial.println(chibiGetShortAddr(), HEX);

  // set channel number
  chibiSetChannel(CHANNEL);
  strcpy_P(tmp, PSTR("Just set channel to "));
  Serial.print(tmp);
  Serial.println(CHANNEL);

  // initialize the device array (all zero)
  memset(devices, 0, MAX_DEV_NUM*sizeof(BGeigieDev));

  // init sleep mode
  bg_pwr_init(pinSwitch, power_up, shutdown);

  Serial.println("Go!");
}

/**************************************************************************/
// Loop
/**************************************************************************/
void loop()
{
  char tmp[50];

  /* manage power mode (on/off) */
  bg_pwr_loop();

  /* RADIO DATA RX */
  
  // Check if any data was received from the radio. If so, then handle it.
  if (chibiDataRcvd() == true)
  { 
    int L; //, rssi, src_addr;
    byte buf[MAX_PKT_SIZE+1] = {0};  // this is where we store the received data
    char line[9] = {0};
    int pos_dollar, pos_star;

    // retrieve the data
    L = chibiGetData(buf);

    // check the size of the data received to avoid buffer overflow
    if (L > MAX_PKT_SIZE)
      buf[MAX_PKT_SIZE] = 0; // null terminate at max length
    else
      buf[L] = 0; // null terminate the string.

    // find the beginning and end of expected sentence
    pos_dollar = find_char((char *)buf, '$', L);
    pos_star   = find_char((char *)buf, '*', L);

    if (pos_dollar != -1 && pos_star != -1)
    {
      // make sure it's a null terminated string
      buf[pos_star+3] = 0;
      buf[pos_star+4] = 0;
      buf[pos_star+5] = 0;
      
      // Print out the message
      Serial.println((char *)buf);
     
      // extract the data from the sentence received
      digitalWrite(led, HIGH);
      delay(50);
      extract_data((char *)(buf+pos_dollar), pos_star-pos_dollar+3);
      digitalWrite(led, LOW);
    }
    else
    {
      strcpy_P(tmp, PSTR("Error: $="));
      Serial.print(tmp);
      Serial.print(pos_dollar);
      strcpy_P(tmp, PSTR(" *="));
      Serial.print(tmp);
      Serial.println(pos_star);
      data_corrupt_flag = 1;
    }

    // if data is corrupt, only display that
    if (data_corrupt_flag == 1)
    {
      /* Checksum didn't match. We choose not to update display */
      /* If you want to display some message, uncomment 4 following lines */
      // lcd.setCursor(0, 0);
      // lcd.print("Bad Data");
      // lcd.setCursor(0, 1);
      // lcd.print("Received");
      strcpy_P(tmp, PSTR("Data was corrupted."));
      Serial.println(tmp);
    }
  }

  /* DISPLAY */

  // select device to display (just in case it changes while
  // we are still updating the diplay)
  int d = dev_index % MAX_DEV_NUM;

  // if device is not active anymore select first active device
  if (!devices[d].active)
  {
    d = -1;
    for (int n = 0 ; n < MAX_DEV_NUM ; n++)
      if (devices[n].active)
      {
        d = n;
        break;
      }
  }
  
  // display selected device (if any)
  if (d != -1)
  { 

    // ready to display the data on screen
    display.clearDisplay();
    char strbuffer[32];
    int offset = 0;

    // Display date
    sprintf_P(strbuffer, PSTR("%04d-%02d-%02d %02d:%02d"),  \
        devices[d].year, devices[d].month, devices[d].day, \
        devices[d].hour, devices[d].minute);
    display.setCursor(2, offset+32); // textsize*8 
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.println(strbuffer);

    // Display CPM
    display.setCursor(2, offset);
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.print("CPM ");
    display.println(devices[d].CPM);

    // Display SD, GPS and Geiger states
    /*
    if (openlog_ready) {
      display.setTextColor(WHITE);
    } else {
      display.setTextColor(BLACK, WHITE); // 'inverted' text
    }
    */
    
    // print device id
    display.setCursor(92, offset);
    display.setTextSize(1);
    display.print("Dev");
    sprintf_P(strbuffer, PSTR("%03u"), devices[d].id);
    display.println(strbuffer);

    // print GPS status (number of satellites if lock)
    if (devices[d].gps_flag == 'A')
    {
      display.setCursor(92, offset+8);
      display.setTextSize(1);
      display.print("GPS ");
      display.println(devices[d].num_sat);
    }
    else
    {
      display.setCursor(92, offset+8);
      display.setTextSize(1);
      display.println("No GPS");
    }

    // print a message when radiation not ready
    if (devices[d].rad_flag != 'A')
    {
      display.setCursor(86, offset+16);
      display.setTextSize(1);
      display.println("Wait...");
    }

    // Display uSv/h
    sprintf_P(strbuffer, PSTR("%lu.%03u"), devices[d].uSh_int, devices[d].uSh_dec);
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(2, offset+16); // textsize*8 
    display.print(strbuffer);
    display.println(" uSv/h");

    display.display();
    
    // Status info on second line
    /*
    lcd.setCursor(0, 1);
    if (battery != -1 && battery < 3400)
    {
      strcpy_P(line, PSTR("LOW BATT"));
    }
    else if (hv != -1 && hv < 450)
    {
      strcpy_P(line, PSTR("HV LOW"));
    }
    else if (gps_flag == 'V')
    {
      strcpy_P(line, PSTR("NOGPS"));
      strcpy(line+5, dev_id);
    }
    else if (sd_status == 0)
    {
      strcpy_P(line, PSTR("SD FAIL!"));
    }
    else if (gps_flag == 'A')
    {
      strcpy_P(line, PSTR("GPS  "));
      strcpy(line+5, dev_id);
    }
    else
    {
      strcpy_P(line, PSTR("STRANGE!"));
    }

    lcd.print(line);
*/


  }
  else
  {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(2, 2);
    display.print("No device connected.");
    display.display();

  }

  /* RADIO LINK STATUS UPDATE */

  for (d = 0 ; d < MAX_DEV_NUM ; d++)
  {
    unsigned long T;
    if (millis() - devices[d].timestamp < 0)
      T = ULONG_MAX - millis() + devices[d].timestamp;
    else 
      T = millis() - devices[d].timestamp;

    if (devices[d].active && T > DEV_TIMEOUT)
    {
      devices[d].active = 0;
      // ring the alarm if this is the device that is selected
      if (d == dev_index)
        buzz(4000, 1, 1000, 0);
    }
  }

}


/*
 * Power up and shutdown routines
 */

void power_up()
{
  // turn on USB
  USBDevice.attach();

  // initialize the device array
  memset(devices, 0, MAX_DEV_NUM*sizeof(BGeigieDev));
  dev_index = 0;

  // configure SPI port directions
  pinMode(SCK, OUTPUT);
  pinMode(MOSI, OUTPUT);
  pinMode(MISO, INPUT);
  pinMode(SS, OUTPUT); // this is needed to ensure the uC is always a master on SPI

  // Turn radio on
  chibiSleepRadio(0);

  // set pins for buzzer and LED
  pinMode(buzzerPin, OUTPUT);
  pinMode(led, OUTPUT);

  // setup display power
  pinMode(OLED_CLK, OUTPUT);
  pinMode(OLED_CS, OUTPUT);
  pinMode(OLED_RESET, OUTPUT);
  pinMode(OLED_DC, OUTPUT);
  pinMode(OLED_DATA, OUTPUT);

  pinMode(OLED_PWR, OUTPUT);
  digitalWrite(OLED_PWR, LOW);
  
  // re-initialize display
  display.begin(SSD1306_SWITCHCAPVCC);

  // Display some kind of splash screen
  display.clearDisplay();   // clears the screen and buffer
  display.setCursor(2, 30);
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.println("SAFECAST");
  display.setTextSize(1);
  sprintf_P(strbuffer, PSTR("Ninja v%s"), version);
  display.print(strbuffer);
  display.display();
  delay(2000);
}

void shutdown()
{
  // Say good bye...
  display.clearDisplay();   // clears the screen and buffer
  display.setCursor(2, 30);
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.println("Good Bye.");
  display.display();
  delay(1000);

  // clear display before sleep
  display.clearDisplay();
  display.display();

  // turn display off
  digitalWrite(OLED_PWR, HIGH);
  pinMode(OLED_CLK, INPUT);
  pinMode(OLED_CS, INPUT);
  pinMode(OLED_RESET, INPUT);
  pinMode(OLED_DC, INPUT);
  pinMode(OLED_DATA, INPUT);
  

  // make all pins inputs to save power
  /*
  for (int i = 0 ; i < NUM_DIGITAL_PINS ; i++)
  {
    pinMode(i, INPUT);    // input
    digitalWrite(i, LOW); // no pull up
  }
  */

  // Radio off
  chibiSleepRadio(1);

  USBDevice.detach(); // if that ever becomes necessary
}

/*
 * Find first occurence of 'c' within
 * the first N characters of buf
 */
int find_char(char *buf, char c, int N)
{
  int pos = 0;
  while (pos < N && buf[pos] != c)
    pos++;
  if (pos < N && buf[pos] == c)
    return pos;
  else
    return -1;
}

/* initialize a device */
void init_device(int d, uint16_t id)
{
  // Link status and age
  devices[d].timestamp = millis();
  devices[d].active = 1;

  // device id
  devices[d].id = id;

  // status variables
  devices[d].CPM = 0;
  devices[d].total = 0;
  devices[d].uSh_int = 0;
  devices[d].uSh_dec = 0;
  devices[d].rad_flag = 'V';
  devices[d].gps_flag = 'V';
  devices[d].num_sat = 0;

  // diagnostic variables
  devices[d].temperature = TEMP_INVALID;
  devices[d].humidity = -1;
  devices[d].battery = -1;
  devices[d].vcc = -1;
  devices[d].hv = -1;
  devices[d].sd_status = 1;

  // the date and time
  devices[d].year = 0;
  devices[d].day = 0;
  devices[d].month = 0;
  devices[d].hour = 0;
  devices[d].minute = 0;
  devices[d].second = 0;


}

/* Find device index in array, or return -1 */
int find_device(uint16_t id)
{
  int d;

  // see if device exists
  for (int d = 0 ; d < MAX_DEV_NUM ; d++)
    if (devices[d].id == id)
      return d;

  // if we get here device is not in array
  // find free spot
  for (int d = 0 ; d < MAX_DEV_NUM ; d++)
    if (!devices[d].active)
    {
      init_device(d, id);
      return d;
    }

  // if we get here, device is not in array
  // and the array is full, then ignore...
  return -1;
}

void extract_data(char *buf, int L)
{
  int i;
  char field[L];
  char *cpm;
  char *tot;
  char r_flag;
  char g_flag;
  char tmp[25];

  // assume data is good
  data_corrupt_flag = 0;

  // verify NMEA formatting of received sentence
  if (!gps_verify_NMEA_sentence(buf, L))
  {
    data_corrupt_flag = 1;
    strcpy_P(tmp, PSTR("NMEA format check failed."));
    Serial.print(tmp);
    return;
  }

  // tokenize line
  char *tok[SYM_SZ] = {0};
  int obj_num = 0;
  char *string;
  string = buf;
  while ((tok[obj_num++] = strsep(&string, ",")) != NULL)
    ;

/*
  // could be useful for debug
  j--;
  strcpy_P(tmp, PSTR("Tokenized "));
  Serial.print(tmp);
  Serial.print(j);
  strcpy_P(tmp, PSTR(" objects."));
  Serial.println(tmp);

  for (i=0 ; i < j ; i++)
  {
    Serial.print(i);
    Serial.print(" : ");
    Serial.println(tok[i]);
  }
  */

  int d;
  unsigned int id;

  // first getting device id
  if (obj_num > 2 && tok[1][0] != 0)
  {
    id = strtoul(tok[1], NULL, 10);
    d = find_device(id);
  }
  else
  {
    // device identification failure
    d = -1;
  }

  if (d != -1)
  {
    // mark device as active and reset timestamp
    devices[d].id = id;
    devices[d].active = 1;
    devices[d].timestamp = millis();

    // parse the line for useful content according to label
    if (obj_num >= 13 && 
      (strcmp_P(tok[0], PSTR("$BMRDD")) == 0 || strcmp_P(tok[0], PSTR("$BNXRDD")) == 0))
    {

      if (tok[2][0] != 0)
        sscanf_P(tok[2], PSTR("%hd-%hu-%huT%hu:%hu:%huZ"), 
          &(devices[d].year), &(devices[d].month), &(devices[d].day), 
          &(devices[d].hour), &(devices[d].minute), &(devices[d].second));
      else
        devices[d].year = devices[d].month = devices[d].day 
          = devices[d].hour = devices[d].minute = devices[d].second = 0;

      if (tok[6][0] != 0)
        r_flag = tok[6][0];  // radiation count available flag
      else
        r_flag = 'V';

      if (tok[12][0] != 0)
        g_flag = tok[12][0]; // GPS location available flag
      else
        g_flag = 'V';

      if (tok[3][0] != 0)
        devices[d].CPM = strtoul(tok[3], NULL, 10);
      else
        devices[d].CPM = 0;

      if (tok[5][0] != 0)
        devices[d].total = strtoul(tok[5], NULL, 10);
      else
        devices[d].total = 0;

      // save uSv/h value (AVOID USING FLOAT TO SAVE SPACE ON FLASH)
      devices[d].uSh_int = devices[d].CPM/LND7313_CONVERSION_FACTOR;
      devices[d].uSh_dec = ((devices[d].CPM*1000)/LND7313_CONVERSION_FACTOR) 
                            - devices[d].uSh_int*LND7313_CONVERSION_FACTOR;


      /* buzz if rad flag becomes not valid */
      if (devices[d].rad_flag == 'A' && r_flag == 'V')
        buzz(3500, 4, 50, 50);
      else if (devices[d].rad_flag == 'V' && r_flag == 'A')
        buzz(3500, 1, 50, 1);
      devices[d].rad_flag = r_flag;

      /* buzz if gps flag becomes not valid */
      if (devices[d].gps_flag == 'A' && g_flag == 'V')
        buzz(4500, 4, 50, 50);
      else if (devices[d].gps_flag == 'V' && g_flag == 'A')
        buzz(4500, 1, 50, 0);
      devices[d].gps_flag = g_flag;

    }
    else if (obj_num >= 14 && strcmp(tok[0], "$BNXSTS") == 0)
    {

      if (tok[7][0] != 0)
        devices[d].num_sat = strtol(tok[7], NULL, 10);
      else
        devices[d].num_sat = 0;

      // temperature reading
      if (tok[9][0] != 0)
        devices[d].temperature = strtol(tok[9], NULL, 10);
      else
        devices[d].temperature = TEMP_INVALID;

      // humidity reading
      if (tok[10][0] != 0)
        devices[d].humidity = strtol(tok[10], NULL, 10);
      else
        devices[d].humidity = -1;

      // battery
      if (tok[11][0] != 0)
        devices[d].battery = strtol(tok[11], NULL, 10);
      else
        devices[d].battery = -1;

      // Geigier Tube high voltage (only available with allan's board so far)
      if (tok[12][0] != 0)
        devices[d].hv = strtol(tok[12], NULL, 10);
      else
        devices[d].hv = -1;

      // SD card status
      if (tok[13][0] != '1' || tok[14][0] != '1' || tok[15][0] != '1')
        devices[d].sd_status = 0;
      else
        devices[d].sd_status = 1;

    }
    else if (strcmp(tok[0], "$BMSTS") == 0)
    {

      // battery
      if (tok[8][0] != 0)
        devices[d].battery = strtol(tok[8], NULL, 10);
      else
        devices[d].battery = -1;

      // VCC of bGeigie (only PlusShield so far)
      if (tok[9][0] != 0)
        devices[d].vcc = strtol(tok[9], NULL, 10);
      else
        devices[d].vcc = -1;

      // SD card status
      if (tok[10][0] != '1' || tok[11][0] != '1' || tok[12][0] != '1')
        devices[d].sd_status = 0;
      else
        devices[d].sd_status = 1;

    }
    else
    {
      // raise flag
      data_corrupt_flag = 1;
      // mark device as inactive
      devices[d].active = 0;
      //Serial.print("NMEA format not recognized.");
      return;
    }

  }

}

void buzz(int f, int p, int t_up, int t_dw)
{
  if (p == 0)
    return;

  while (p > 0)
  {
    tone(buzzerPin, f);
    delay(t_up);
    noTone(buzzerPin);
    p--;
    if (p > 0)
      delay(t_dw);
  }
}

char gps_checksum(char *s, int N)
{
  int i = 0;
  char chk = s[0];

  for (i=1 ; i < N ; i++)
    chk ^= s[i];

  return chk;
}

// compare checksum of string str of length L
// with hex checksum contained in length 2 string chk
int gps_checksum_match(char *str, int L, char *chk)
{
  char ch1, ch2;

  // compute local checksum
  char chk_str = gps_checksum(str, L);

  // transform hex string to char
  // first digit
  if (chk[0] > '9')
    ch1 = chk[0] - 'A' + 10;
  else
    ch1 = chk[0] - '0';
  // second digit
  if (chk[1] > '9')
    ch2 = chk[1] - 'A' + 10;
  else
    ch2 = chk[1]-'0';
  char chk_num = ch1*16 + ch2;

  // return result of matching
  return (chk_str == chk_num);
}

// verify formating and checksum of NMEA-style sentence of length L
int gps_verify_NMEA_sentence(char *sentence, int L)
{
  // basice property, starts with dollar, ends with star
  if (sentence[0] != '$' || sentence[L-3] != '*')
    return false;

  // verify checksum is hex string
  if ( ! ( (sentence[L-2] >= '0' && sentence[L-2] <= '9') || (sentence[L-2] <= 'F' && sentence[L-2] >= 'A') ) )
    return false;
  if ( ! ( (sentence[L-1] >= '0' && sentence[L-1] <= '9') || (sentence[L-1] <= 'F' && sentence[L-1] >= 'A') ) )
    return false;

  // if we reach that part, just return checksum matching result
  return gps_checksum_match(sentence+1, L-4, sentence+L-2);
}

int FreeRam () 
{
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}
