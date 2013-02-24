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

#ifndef __BG_SENSORS_H__
#define __BG_SENSORS_H__

#include <Arduino.h>

// Sensor power on/off
extern int bgs_pwr;
#define bg_sensors_on() digitalWrite(bgs_pwr, LOW)
#define bg_sensors_off() digitalWrite(bgs_pwr, HIGH)

// functions for sensors usage
void bgs_sensors_init(int pin_pwr, int pin_batt, int pin_temp, int pin_hum, int pin_hv);
//void bg_sensors_on();
//void bg_sensors_off();
float bgs_read_battery();
float bgs_read_temperature();
float bgs_read_humidity();
float bgs_read_hv();

#endif /* __BG_SENSORS_H__ */

