/*
   Simple library to do soft shutdown and power up based on a momentary switch

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

#ifndef __BGPWR_H__
#define __BGPWR_H__

// Link to arduino library
#if ARDUINO >= 100
#include <Arduino.h>
#else
#include <WProgram.h>
#endif

// State information variable
#define BG_STATE_PWR_DOWN   0
#define BG_STATE_PWR_UP     1

// main state variable
extern int bg_pwr_state;
// this flag decides if we need to execute
// wake up and sleep routines.
extern int bg_pwr_exec_sleep_routine_flag;

// the press time needed to turn on or off the bGeigie (ms)
#define BG_PWR_BUTTON_TIME 1000

// main pin setup routine
void bg_pwr_init(int pin_switch, void (*on_wakeup)(), void (*on_sleep)());
void bg_pwr_setup_switch_pin();

// for button handling
extern int bg_pwr_button_pressed_flag;
extern unsigned long bg_pwr_button_pressed_time;

// routine to execute in the loop
void bg_pwr_loop();

// set state to off (effectively turns device off
void bg_pwr_turn_off();

// check if device is running or powered down
int bg_pwr_running();

// sleep routing 
void bg_pwr_down();

#endif /* __BGPWR_H__ */
