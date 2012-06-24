
#ifndef __BGEIGIE_H__
#define __BGEIGIE_H__

#include <bg_pins.h>

// State information variable
#define BG_STATE_PWR_DOWN 0
#define BG_STATE_SENSOR 1
#define BG_STATE_SD_READER 2
static bg_pwr_state;

static char msg_device_id[] PROGMEM = "Device Id: ";
static char msg_device_id_invalid[] PROGMEM = "Device Id invalid\n";

// main pin setup routine
void bg_pwr_init();
void bg_pwr_setup_switch_pin()

// power up and shutdown
void bg_pwr_up();
void bg_pwr_down();

// main switch interrupt service routine
void bg_pwr_isr();
void bg_pwr_init();

#endif /* __BGEIGIE_H__ */
