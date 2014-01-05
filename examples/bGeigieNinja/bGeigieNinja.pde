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


#include <SD.h>
#include <SPI.h>

#include <chibi.h>
#include <GPS.h>

#include "version.h"

/* device id length */
#define BMRDD_ID_LEN 3

#define DIM_TIME 60000
#define DIM_LEN 1000

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <LiquidCrystal.h>

#define RX_ADDR 0x1234
#define CHANNEL 20

static const int radioSelect = A3;

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(2, 3, 4, 5, 6, 7);
// pin layout
int backlightPin = 9;
int buzzerPin = 8;
int pinLightSensor = 0;
int pinTiltSensor = 15;

// status variables
unsigned long CPM;
unsigned long total;
double uSh_pre = 0;
char rad_flag = 'A';
char gps_flag = 'A';
char data_corrupt_flag = 0;
char dev_id[BMRDD_ID_LEN+1];  // device id

// diagnostic variables
int temperature = -1;
int humidity = -1;
int battery = -1;
int hv = -1;
int sd_status = 1;
int vcc = -1; // VCC of PlusShield

// link status variable
char lnk_flag = 'X';
long timeout = 15000;
long last_msg_time = 0;

// screen variables
float brightness;
unsigned long dimmerTimer;
int dimmed;
int tilt_pre;

/**************************************************************************/
// Initialize
/**************************************************************************/
void setup()
{  
  // this is needed to ensure the uC is always a master on SPI
  pinMode(10, OUTPUT);
  // the radio chip select is also define in chibiArduino library
  // but let's make it explicit
  pinMode(radioSelect, OUTPUT);

  // set pins
  pinMode(backlightPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(pinTiltSensor, INPUT);
  
  // set brightness
  setBrightness(1.0f);
  dimmed = 0;
  tilt_pre = 0;
  
  // set up the LCD's number of columns and rows: 
  lcd.begin(8, 2);

  // Print a message to the LCD.
  lcd.print("SAFECAST");
  lcd.setCursor(0, 1);
  lcd.print("Welcome!");
  lcd.setCursor(0, 1);
  buzz(4000, 2, 50, 150);
  delay(2000);

  // Print version number
  lcd.setCursor(0, 0);
  lcd.print("Ninja   ");
  lcd.setCursor(0, 1);
  lcd.print("        ");
  lcd.setCursor(0, 1);
  lcd.print("v");
  lcd.print(version);
  delay(2000);

  // Initialize the chibi command line and set the speed to 9600 bps
  Serial.begin(57600);

  Serial.print("Safecast bGeigie Ninja, version ");
  Serial.println(version);

  // Initialize the chibi wireless stack
  chibiInit();

  Serial.println("Init chibi done.");

  // set address
  chibiSetShortAddr(RX_ADDR);
  Serial.print("Just set address to 0x");
  Serial.println(chibiGetShortAddr(), HEX);

  // set channel number
  chibiSetChannel(CHANNEL);
  Serial.print("Just set channel to ");
  Serial.println(CHANNEL);

}

/**************************************************************************/
// Loop
/**************************************************************************/
void loop()
{
  // Check if any data was received from the radio. If so, then handle it.
  if (chibiDataRcvd() == true)
  { 
    int L; //, rssi, src_addr;
    byte buf[150] = {0};  // this is where we store the received data
    char line[9] = {0};
    int pos_dollar, pos_star;

    // retrieve the data
    L = chibiGetData(buf);

    // find the beginning and end of expected sentence
    pos_dollar = find_char((char *)buf, '$', L);
    pos_star   = find_char((char *)buf, '*', L);

    if (pos_dollar != -1 && pos_star != -1)
    {
      // make sure it's a null terminated string
      buf[pos_star+3] = 0;
      
      // Print out the message
      Serial.println((char *)buf);
     
      // set time of message received
      last_msg_time = millis();
      
      // set the link flag to OK
      lnk_flag = 'O';

      // extract the data from the sentence received
      extract_data((char *)(buf+pos_dollar), pos_star-pos_dollar+3);
    }
    else
    {
      Serial.print("Error: $=");
      Serial.print(pos_dollar);
      Serial.print(" *=");
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
      Serial.println("Data was corrupted.");
    }
    else
    { // If data received is not corrupted (checksum matches)

      // compute dose rate
      double uSh = CPM/350.0;

      // compute dose
      //double dose = total/350.0/60.0;
      
      // dose rate on first row
      uShStr(CPM, line, 350);
      lcd.setCursor(0, 0);
      if (rad_flag == 'A')
        lcd.print(line);
      else if (CPM != 0)
        lcd.print("Wait... ");
      else
        lcd.print("NoGeiger");

      // Status info on second line
      lcd.setCursor(0, 1);
      if (sd_status == 0)
      {
        strcpy_P(line, PSTR("SD FAIL!"));
      }
      else if (battery != -1 && battery < 3400)
      {
        strcpy_P(line, PSTR("LOW BATT"));
      }
      else if (hv != -1 && hv < 450)
      {
        strcpy_P(line, PSTR("HV LOW  "));
      }
      else if (gps_flag == 'V')
      {
        strcpy_P(line, PSTR("NOGPS"));
        strcpy(line+5, dev_id);
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

      if (uSh_pre < 0.5 && uSh >= 0.5)
        buzz(2000, 2, 500, 500);
      else if (uSh_pre < 1.0 && uSh >= 1.0)
        buzz(2000, 4, 250, 250);
      else if (uSh_pre < 3.0 && uSh >= 3.0)
        buzz(2000, 8, 125, 125);
      else if (uSh_pre < 5.0 && uSh >= 5.0)
        buzz(2000, 16, 65, 65);

      // update uSievert/hour memory
      uSh_pre = uSh;

    }

  } else {

    long now = millis();
    if ((lnk_flag == 'O') && (now - last_msg_time > timeout))
    {
      // alarm
      buzz(4000, 1, 1000, 1);
      // update link status
      lnk_flag = 'X';
      // print error message
      lcd.setCursor(0, 0);
      lcd.print("No Link ");
      lcd.setCursor(0, 1);
      lcd.print("Check BG");
    }
  }

  controlBrightness();
}


/*
 * Find first occurence of 'c' within
 * the first N characters of buf
 */
int find_char(char *buf, char c, int N)
{
  int pos = 0;
  while (buf[pos] != c && pos < N)
    pos++;
  if (pos < N && buf[pos] == c)
    return pos;
  else
    return -1;
}


void extract_data(char *buf, int L)
{
  int i;
  char field[L];
  char *cpm;
  char *tot;
  char *r_flag;
  char *g_flag;

  // assume data is good
  data_corrupt_flag = 0;

  // verify NMEA formatting of received sentence
  if (!gps_verify_NMEA_sentence(buf, L))
  {
    data_corrupt_flag = 1;
    Serial.print("NMEA format check failed.");
    return;
  }

  // tokenize line
  char *tok[SYM_SZ] = {0};
  int j = 0;
  char *string;
  string = buf;
  while ((tok[j++] = strsep(&string, ",")) != NULL)
    ;
 
  // parse the line for useful content according to label
  if (strcmp(tok[0], "$BMRDD") == 0 || strcmp(tok[0], "$BNXRDD") == 0)
  {

    // first getting device id
    memset(dev_id, 0, 4);
    if (tok[1][0] != 0)
      memcpy(dev_id, tok[1], 3);

    r_flag = tok[6];  // radiation count available flag
    g_flag = tok[12]; // GPS location available flag

    if (tok[3][0] != 0)
      CPM = strtoul(tok[3], NULL, 10);
    else
      CPM = 0;

    if (tok[5][0] != 0)
      total = strtoul(tok[5], NULL, 10);
    else
      total = 0;

    /* buzz if rad flag becomes not valid */
    if (rad_flag == 'A' && r_flag[0] == 'V')
      buzz(4000, 4, 100, 150);
    else if (rad_flag == 'V' && r_flag[0] == 'A')
      buzz(4000, 1, 50, 1);
    rad_flag = r_flag[0];

    /* buzz if gps flag becomes not valid */
    if (gps_flag == 'A' && g_flag[0] == 'V')
      buzz(8000, 4, 100, 150);
    else if (gps_flag == 'V' && g_flag[0] == 'A')
      buzz(8000, 1, 50, 0);
    gps_flag = g_flag[0];

  } 
  else if (strcmp(tok[0], "$BNXSTS") == 0 && strncmp(tok[1], dev_id, 3) == 0)
  {

    // temperature reading
    if (tok[9][0] != 0)
      temperature = atoi(tok[9]);
    else
      temperature = -1;

    // humidity reading
    if (tok[10][0] != 0)
      humidity = atoi(tok[10]);
    else
      humidity = -1;

    // battery
    if (tok[11][0] != 0)
      battery = atoi(tok[11]);
    else
      battery = -1;

    // high voltage
    if (tok[12][0] != 0)
      hv = atoi(tok[12]);
    else
      hv = -1;

    // SD card status
    if (tok[13][0] != '1' || tok[14][0] != '1' || tok[15][0] != '1')
      sd_status = 0;
    else
      sd_status = 1;

  }
  else if (strcmp(tok[0], "$BMSTS") == 0)
  {

    // battery
    if (tok[8][0] != 0)
      battery = strtol(tok[8], NULL, 10);
    else
      battery = -1;

    // VCC of bGeigie (only PlusShield so far)
    if (tok[9][0] != 0)
      vcc = strtol(tok[9], NULL, 10);
    else
      vcc = -1;

    // SD card status
    if (tok[10][0] != '1' || tok[11][0] != '1' || tok[12][0] != '1')
      sd_status = 0;
    else
      sd_status = 1;

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

void uShStr(unsigned long CPM, char *line, int F)
{
  int d;
  int i = CPM/F;
  int p = 0;
  int n;

  // dose rate on first row
  if (i < 10)
  {
    n = 1;
    d = (int)(((float)CPM/(float)F - i)*1000);
    if (d < 100)
      p = 1;
    else if (d < 10)
      p = 2;
  } else if (i < 100) {
    n = 2;
    d = (int)(((float)CPM/(float)F - i)*100);
    if (d < 10)
      p = 1;
  } else if (i < 1000) {
    n = 3;
    d = (int)(((float)CPM/(float)F - i)*10);
  } else {
    strcpy(line, "*DANGER*");
    return;
  }
  
  if (p == 2)
    sprintf(line, "%d.00%duSh", i, d);
  else if (p == 1)
    sprintf(line, "%d.0%duSh", i, d);
  else
    sprintf(line, "%d.%duSh", i, d);
  return;
}

void setBrightness(float c)
{
  analogWrite(backlightPin, (int)(c*255));
}

void controlBrightness()
{
  float dim_coeff;
  int tilt;
  unsigned long delta_t = millis() - dimmerTimer;

  // read tilt sensors
  tilt = digitalRead(pinTiltSensor);

  // check if dim reset
  if (tilt != tilt_pre)
  {
    dimmerTimer = millis();
    dimmed = 0;
  }
  tilt_pre = tilt;

  // set a priori brightness
  dim_coeff = 1.0;
  
  // now handle dimming
  if (!dimmed)
  {
    if (delta_t > DIM_TIME && delta_t < DIM_TIME + DIM_LEN)
    {
      dim_coeff *= (1.0 - ((float)delta_t - (float)DIM_TIME)/(float)DIM_LEN);
    }
    else if (delta_t >= DIM_TIME + DIM_LEN)
    {
      dim_coeff = 0.0;
      dimmed = 1;
    }
  } else {
    dim_coeff = 0.0;
  }

  setBrightness(dim_coeff);
}

/*
int FreeRam () 
{
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}
*/
