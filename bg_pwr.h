
#ifndef __BGEIGIE_H__
#define __BGEIGIE_H__

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
static int bg_pwr_button_pressed_flag = 0;
static unsigned long bg_pwr_button_pressed_time = 0;

// routine to execute in the loop
void bg_pwr_loop();

// check if device is running or powered down
int bg_pwr_running();

// sleep routing 
void bg_pwr_down();

#endif /* __BGEIGIE_H__ */
