
#include "blinky.h"

/* global variables definition */
byte blink_overflow_counter = 0;
byte blink_mode = BLINK_OFF;

/* the interrupt service routine */
ISR(TIMER2_OVF_vect)
{
  
  blink_overflow_counter = (blink_overflow_counter + 1) % BLINK_OVF_HALFPERIOD;

  if (blink_overflow_counter == 0)
    bg_led_toggle();

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
  else if (blink_mode == BLINK_ON)
  {
    // turn LED off
    bg_led_on();
    return;
  }

  // turn LED on
  bg_led_on();

  // Sets overflow interrupt
  TIMSK2 |= (1 << TOIE2);

  // start counter
  if (blink_mode == BLINK_ALL_OK)
    TCCR2B |= (1 << CS22) | (1 << CS21);  // 1s blink period (clk/256)
  else if (blink_mode == BLINK_BATTERY_LOW)
    TCCR2B |= (1 << CS22) | (1 << CS20);  // 500ms blink period (clk/128)
  else if (blink_mode == BLINK_PROBLEM)
    TCCR2B |= (1 << CS21) | (1 << CS20);  // 125ms blink period (clk/32)
  else
    bg_led_off();

}

