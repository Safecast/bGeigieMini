
#include "blinky.h"

/* global variables definition */
byte blink_overflow_counter = 0;
byte blink_mode = BLINK_OFF;

/* the interrupt service routine */
ISR(TIMER2_OVF_vect)
{
  
  blink_overflow_counter = (blink_overflow_counter + 1) % BLINK_OVF_HALFPERIOD;

  switch (blink_mode)
  {
    case BLINK_ALL_OK:
      if (blink_overflow_counter == 0)
        bg_led_toggle();
      break;

    case BLINK_BATTERY_LOW:
      if (blink_overflow_counter == 0 || blink_overflow_counter == 24)
        bg_led_on();
      else if (blink_overflow_counter == 12 || blink_overflow_counter == 36)
        bg_led_off();
      break;

    case BLINK_PROBLEM:
      if (blink_overflow_counter%(BLINK_OVF_HALFPERIOD/2) == 0)
        bg_led_toggle();
      break;
  }

}

/* LED mode selector */
void blinky(byte mode)
{
  if (mode == blink_mode)
    return;

  // set new mode
  blink_mode = mode;
  bg_led_config();
  blink_overflow_counter = 0;

  // set the timer2 off
  TCCR2A = 0x0;
  TCCR2B = 0x0;
  TCNT2 = 0x0;

  if (blink_mode == BLINK_OFF)
  {
    // turn LED off
    bg_led_off();
    return;
  }

  // then turn LED on
  bg_led_on();

  if (blink_mode == BLINK_ON)
    return;

  // Sets overflow interrupt
  TIMSK2 |= (1 << TOIE2);

  // start counter
  if (blink_mode == BLINK_ALL_OK || blink_mode == BLINK_BATTERY_LOW)
    TCCR2B |= (1 << CS22) | (1 << CS21);  // 1s blink period (clk/256)
  else if (blink_mode == BLINK_PROBLEM)
    TCCR2B |= (1 << CS21) | (1 << CS20);  // 125ms blink period (clk/32)
  else
    bg_led_off();
  /*
  else if (blink_mode == BLINK_BATTERY_LOW)
    TCCR2B |= (1 << CS22) | (1 << CS20);  // 500ms blink period (clk/128)
    */

}

