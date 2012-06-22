
#ifndef __BGEIGIE_H__
#define __BGEIGIE_H__

#include <bg_pins.h>

/* we use 2.5V internal reference */
#define VREF 2.5

// State information variable
#define BG_STATE_PWR_DOWN 0
#define BG_STATE_SENSOR 1
#define BG_STATE_SD_READER 2
static state;

// turn on and off different bits of hardware
#define bg_gps_on() digitalWrite(gps_on_off, LOW)
#define bg_gps_off() digitalWrite(gps_on_off, HIGH)
#define bg_sensors_on() digitalWrite(sense_pwr, LOW)
#define bg_sensors_off() digitalWrite(sense_pwr, HIGH)
#define bg_sd_on() digitalWrite(sd_pwr, LOW)
#define bg_sd_off() digitalWrite(sd_pwr, HIGH)
#define bg_hvps_on() digitalWrite(hvps_pwr, LOW)
#define bg_hvps_off() digitalWrite(hvps_pwr, HIGH)

// check if SD card is inserted
#define bg_sd_card_present() (!digitalRead(sd_detect))

// LED on, off
#define bg_led_on() digitalWrite(led, HIGH)
#define bg_led_off() digitalWrite(led, LOW)

// Various messages that we store in flash
#define BG_ERR_MSG_SIZE 40
static char msg_sd_not_present[] PROGMEM = "SD not inserted.\n";
static char msg_sd_init[] PROGMEM = "SD init...\n";
static char msg_card_failure[] PROGMEM = "Card failure...\n";
static char msg_card_read[] PROGMEM = "SD card ready\n";
static char msg_error_log_file_cannot_open[] PROGMEM = "Error: Log file cannot be opened.\n";
static char msg_device_id[] PROGMEM = "Device Id: ";
static char msg_device_id_invalid[] PROGMEM = "Device Id invalid\n";

// main pin setup routine
void bg_pins_setup();

// power up and shutdown
void bg_powerup();
void bg_shutdown();

// main switch interrupt service routine
void bg_isr_main_switch();
void bg_main_switch_config();

// functions for sensors usage
void bg_sensors_config();
float bg_read_battery();
float bg_read_temperature();
float bg_read_humidity();
float bg_read_hvps();

// functions related to SD card operations
int bg_sd_initialized;
int bg_sd_inserted;
int bg_sd_last_write;
void bg_sd_card_init();
void bg_sd_writeln(char *filename, char *log);

#endif /* __BGEIGIE_H__ */
