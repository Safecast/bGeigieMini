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


#include <chibi.h>

/* device id length */
#define BMRDD_ID_LEN 3

#define ENABLE_LCD 1
#define DIM_TIME 60000
#define DIM_LEN 1000

#if ENABLE_LCD
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <LiquidCrystal.h>
#endif

#define RX_ADDR 0x1234
#define CHANNEL 20

static const int radioSelect = A3;

#if ENABLE_LCD
// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(2, 3, 4, 5,6,7);
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
char dev_id[BMRDD_ID_LEN+1];  // device id

// link status variable
char lnk_flag = 'X';
long timeout = 15000;
long last_msg_time = 0;

// screen variables
float brightness;
unsigned long dimmerTimer;
int dimmed;
int tilt_pre;
#endif

/**************************************************************************/
// Initialize
/**************************************************************************/
void setup()
{  
  // this is needed to ensure the uC is always a master on SPI
  pinMode(SS, OUTPUT);
  // the radio chip select is also define in chibiArduino library
  // but let's make it explicit
  pinMode(radioSelect, OUTPUT);

#if ENABLE_LCD
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
  buzz(4000, 2, 50, 150);
  delay(2000);
#endif

  // Initialize the chibi command line and set the speed to 9600 bps
  Serial.begin(9600);

  Serial.println("Hello.");

  // Initialize the chibi wireless stack
  chibiInit();

  Serial.println("Init chibi done. Now go. good.");

  Serial.println("All good.");
  
  // set address
  chibiSetShortAddr(RX_ADDR);
  Serial.print("Just set address to ");
  Serial.println(chibiGetShortAddr());

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
    int len, rssi, src_addr;
    byte buf[150] = {0};  // this is where we store the received data
    char line[9] = {0};
    
    // retrieve the data
    len = chibiGetData(buf);
    
    // Print out the message
    Serial.println((char *)buf);
   
#if ENABLE_LCD

    // set time of message received
    last_msg_time = millis();
    
    // set the link flag to OK
    lnk_flag = 'O';

    // first get CPM and Dose
    int L = strlen((char *)buf);

    // extract the data from the sentence received
    extract_data((char *)buf, L);

    // compute dose rate
    double uSh = CPM/350.0;

    // compute dose
    double dose = total/350.0/60.0;
    
    // dose rate on first row
    uShStr(CPM, line, 350);
    lcd.setCursor(0, 0);
    if (rad_flag == 'A')
      lcd.print(line);
    else if (CPM != 0)
      lcd.print("Wait... ");
    else
      lcd.print("NoGeiger");

    // connection info on the 2nd row
    lcd.setCursor(0, 1);
    if (gps_flag == 'A')
    {
      line[0] = 'G';
      line[1] = 'P';
      line[2] = 'S';
    }
    else
    {
      line[0] = ' ';
      line[1] = ' ';
      line[2] = ' ';
    }
    line[3] = ' ';
    line[4] = ' ';
    line[5] = dev_id[0];
    line[6] = dev_id[1];
    line[7] = dev_id[2];
    line[8] = NULL;
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
#endif
  }

#if ENABLE_LCD
  controlBrightness();
#endif
}

#if ENABLE_LCD

void extract_data(char *buf, int L)
{
  int i;
  char field[L];
  char *cpm;
  char *tot;
  char *r_flag;
  char *g_flag;

  // first getting device id
  if (L >= 9)
  {
    dev_id[0] = buf[7];
    dev_id[1] = buf[8];
    dev_id[2] = buf[9];
  }
  else
  {
    dev_id[0] = 'Y';
    dev_id[1] = 'Y';
    dev_id[2] = 'Y';
  }
  dev_id[3] = 0;
  
  // jumping 32 characters to arrive at CPM field
  strcpy(field, (char *)buf + 32);
  
  cpm = strtok(field, ",");   // cpm field
  strtok(NULL, ",");          // jumping bin count
  tot= strtok(NULL, ",");  // total count
  r_flag = strtok(NULL, ","); // cpm valid flag
  for (i=0 ; i < 5 ; i++)
    strtok(NULL, ",");
  g_flag = strtok(NULL, ","); // gps validity flag

  CPM = strtoul(cpm, NULL, 10);
  total = strtoul(tot, NULL, 10);

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

#endif

