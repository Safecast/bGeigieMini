
#ifndef __BGEIGIE_H__
#define __BGEIGIE_H__

// Link to arduino library
#if ARDUINO >= 100
#include <Arduino.h>
#else
#include <WProgram.h>
#endif

// State information variable
#define BG_STATE_PWR_DOWN 0
#define BG_STATE_PWR_UP   1
#define BG_STATE_SENSOR Â  2
#define BG_STATE_SD_READER 3
static int bg_pwr_state;

// main pin setup routine
void bg_pwr_init(int pin_switch, int interrupt, void (*on_wakeup)(), void (*on_sleep)());
void bg_pwr_setup_switch_pin();

// power up and shutdown
void bg_pwr_up();
void bg_pwr_down();

// main switch interrupt service routine
void bg_pwr_isr();

#endif /* __BGEIGIE_H__ */
