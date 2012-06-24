
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

