/*
   Simple library to do reader a couple of sensors on the Safecast bGeigie

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

#include "bg_sensors.h"

// Link to arduino library
#if ARDUINO >= 100
#include <Arduino.h>
#else
#include <WProgram.h>
#endif

// Pin variables
int bgs_pwr;
static int bgs_batt;
static int bgs_temp;
static int bgs_hum;
static int bgs_hv;

/* we use 2.5V internal reference */
#define VREF 2.56
#define DIVIDER 1024.

// configures sensors
void bgs_sensors_init(int pin_pwr, int pin_batt, int pin_temp, int pin_hum, int pin_hv)
{
  // set pins
  bgs_pwr = pin_pwr;
  bgs_batt = pin_batt;
  bgs_temp = pin_temp;
  bgs_hum = pin_hum;
  bgs_hv = pin_hv;

  // set ADC reference
#if defined(__AVR_ATmega1284P__)
  analogReference(INTERNAL2V56);
#else
  analogReference(INTERNAL);
#endif

  // set sensors switch pin
  pinMode(bgs_pwr, OUTPUT);
  bg_sensors_off();
}

// read battery voltage in Volts
// uses a voltage divider with R1 = 100MOhm and R2 = 120MOhm
float bgs_read_battery()
{
  int bat = analogRead(bgs_batt);
  return bat/DIVIDER * VREF / 120. * 220.;
}

// read voltage from HVPS
float bgs_read_hv()
{
  int hv = analogRead(bgs_hv);
  // the multiplication factor comes from Allan's HV power supply board specs
  return hv/DIVIDER * VREF * (1036./36.) * 7.;
}

// read temperature (LM61, SOT23)
float bgs_read_temperature()
{
  int tmp = analogRead(bgs_temp);
  return (tmp/DIVIDER * VREF * 1000. - 600.)/10.;
}

// read humidity sensor (HIH-5030)
float bgs_read_humidity()
{
  // need temperature to compensate sensor reading
  float temp = bgs_read_temperature();

  // warning, 2.5 internal reference might be just not sufficient
  // to cover the whole sensor output range (up to about 95% at 25
  // degree). If full range is needed, switch to 3.3V reference.
  int hum = analogRead(bgs_hum);
  float hum_stat = ((hum/DIVIDER*VREF)/3.3 - 0.1515)/0.00636;

  // compensate for temperature
  hum_stat = hum_stat/(1.0546 - 0.00216*temp);

  return hum_stat;
}

