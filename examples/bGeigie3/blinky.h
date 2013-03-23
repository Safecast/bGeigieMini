#ifndef __BLINKY_H__
#define __BLINKY_H__

#include <bg3_pins.h>

// blinky stuff
#define BLINK_OFF 0
#define BLINK_ON 1
#define BLINK_ALL_OK 2
#define BLINK_BATTERY_LOW 3
#define BLINK_PROBLEM 4
#define BLINK_OVF_HALFPERIOD 61

extern byte blink_overflow_counter;
extern byte blink_mode;

// function definition
void blinky(byte mode);

#endif /* __BLINKY_H__ */
