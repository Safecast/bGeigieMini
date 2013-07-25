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

#include "bg_pwr.h"

#include <limits.h>

#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/interrupt.h>

#include "sd_reader_int.h"

// main state variable
int bg_pwr_state = BG_STATE_PWR_DOWN;

// this flag decides if we need to execute
// wake up and sleep routines.
int bg_pwr_exec_sleep_routine_flag = 0;

// power switch pin
static int bg_pwr_switch_pin;

// for button handling
int bg_pwr_button_pressed_flag = 0;
unsigned long bg_pwr_button_pressed_time = 0;

unsigned long ellapsed_millis(unsigned long start);

// function pointers for power up and down custom routines
void (*bg_pwr_on_wakeup)() = NULL;
void (*bg_pwr_on_sleep)() = NULL;

// main switch interrupt servic routine
// NOTE to self: timers do not work in ISR
ISR(BG_SWITCH_INT)
{
  // raise a flag when the button is pressed when device is powered up
  if (digitalRead(bg_pwr_switch_pin) == HIGH && bg_pwr_state == BG_STATE_PWR_UP)
  {
    bg_pwr_button_pressed_flag = 1;
    bg_pwr_button_pressed_time = millis();
  }
}

// Configure main switch
void bg_pwr_init(int pin_switch, void (*on_wakeup)(), void (*on_sleep)())
{
  // set pins
  bg_pwr_switch_pin = pin_switch;

  // set up power up routines
  bg_pwr_on_wakeup = on_wakeup;

  // set up power down routine
  bg_pwr_on_sleep = on_sleep;

  // setup interrupt
  bg_pwr_setup_switch_pin();

  // default state is to sleep
  bg_pwr_state = BG_STATE_PWR_DOWN;

  // execute on sleep routing pre-emptively
  bg_pwr_exec_sleep_routine_flag = 1;

  // small delay to let serial and stuff finish
  delay(10);
}

void bg_pwr_setup_switch_pin()
{
  // setup main switch interrupt
  pinMode(bg_pwr_switch_pin, INPUT);
  // setup interrupt routine
  BG_SWITCH_INTP();
}


// routine to execute in the loop
void bg_pwr_loop()
{
  // if the button has been pressed check!
  if (bg_pwr_button_pressed_flag && bg_pwr_state == BG_STATE_PWR_UP)
  {
    if (digitalRead(bg_pwr_switch_pin) == HIGH)
    {
      if (millis() - bg_pwr_button_pressed_time > BG_PWR_BUTTON_TIME)
      {
        bg_pwr_state = BG_STATE_PWR_DOWN;
        bg_pwr_button_pressed_flag = 0;
      }
    }
    else
    {
      bg_pwr_button_pressed_flag = 0;
      bg_pwr_button_pressed_time = 0;
    }
  } 

  // as long as the state is down, we turn off
  while (bg_pwr_state == BG_STATE_PWR_DOWN && !(sd_reader_state == SD_READER_ACTIVE || sd_reader_interrupted))
    bg_pwr_down();
}

// set state to off (effectively turns device off
void bg_pwr_turn_off()
{
  bg_pwr_state = BG_STATE_PWR_DOWN;
}

// check if device is running or powered down
int bg_pwr_running()
{
  return (bg_pwr_state == BG_STATE_PWR_UP);
}

// Shutdown
void bg_pwr_down()
{
  // execute sleep routine
  if (bg_pwr_exec_sleep_routine_flag)
  {
    if (bg_pwr_on_sleep != NULL)
      bg_pwr_on_sleep();
    // the flag is needed because we want to execute sleep
    // routine only when we go from pwr up to pwr down
    bg_pwr_exec_sleep_routine_flag = 0;
  }

  //Shut off ADC, TWI, SPI, Timer0, Timer1, Timer2

  ADCSRA &= ~(1<<ADEN); //Disable ADC
  ACSR |= (1<<ACD); //Disable the analog comparator
  DIDR0 = 0xFF; //Disable digital input buffers on all ADC0-ADC5 pins
  DIDR1 = (1<<AIN1D)|(1<<AIN0D); //Disable digital input buffer on AIN1/0

  power_twi_disable();
  power_spi_disable();
  power_usart0_disable();
  power_timer0_disable();
  power_timer1_disable();
  power_timer2_disable();
#if defined(__AVR_ATmega1284P__)
  power_timer3_disable();
#endif

  //Power down various bits of hardware to lower power usage  
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_mode();

  /*********/
  /* SLEEP */
  /*********/

  // The processor wakes up back here after interrupt

  //Turn on ADC, TWI, SPI, Timer0, Timer1, Timer2
  ADCSRA |= (1<<ADEN); // Enable ADC
  ACSR &= ~(1<<ACD);   // Enable the analog comparator

  // this should be set to reflect real usage of analog pins
  DIDR0 = 0x00;   // Enable digital input buffers on all ADC0-ADC5 pins
  DIDR1 &= ~(1<<AIN1D)|(1<<AIN0D); // Enable digital input buffer on AIN1/0

  power_twi_enable();
  power_spi_enable();
  power_usart0_enable();
  power_timer0_enable();
  power_timer1_enable();
  power_timer2_enable();
#if defined(__AVR_ATmega1284P__)
  power_timer3_enable();
#endif

  // check button press time and handle state
  unsigned long start = millis();
  while ( (ellapsed_millis(start) < BG_PWR_BUTTON_TIME) && (digitalRead(bg_pwr_switch_pin) == HIGH) );
  // if the button is pressed continuously for 2 seconds, swap to on state
  if (ellapsed_millis(start) >= BG_PWR_BUTTON_TIME)
    bg_pwr_state = BG_STATE_PWR_UP;

  // lower the button flag
  bg_pwr_button_pressed_flag = 0;

  // execute wake up routine only if we really woke up
  if (bg_pwr_state == BG_STATE_PWR_UP || sd_reader_interrupted)
  {
    // execute wake up routine if any is defined
    if (bg_pwr_on_wakeup != NULL)
      bg_pwr_on_wakeup();
    // next time we sleep execute sleep routine
    bg_pwr_exec_sleep_routine_flag = 1; 
  }
}

// quick helper that compute securely ellapsed time
unsigned long ellapsed_millis(unsigned long start)
{
  unsigned long now = millis();

  if (now >= start)
    return now - start;
  else
    return (ULONG_MAX + now - start);
}


