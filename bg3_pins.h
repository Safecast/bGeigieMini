
#ifndef __PIN_DEFINITION_H__
#define __PIN_DEFINITION_H__

// Link to arduino library
#if ARDUINO >= 100
#include <Arduino.h>
#else
#include <WProgram.h>
#endif

// Main power switch
static const int main_switch = 0;
// main switch is on pin change interrupt 2
#define BG_SWITCH_INT PCINT1_vect
// enable rising edge
#define BG_SWITCH_INTP() do \
  {                         \
    PCMSK1 |= _BV(PCINT8);  \
    PCICR |= _BV(PCIE1);    \
  }                         \
  while(0)                      

// sensors
static const int batt_sense = 31; //A7;
static const int temp_sense = 30; //A6;
static const int hum_sense = 29; //A5;
static const int hv_sense = 28; //A4;
static const int sense_pwr = 19;
static const int charge_stat = 23;

// Radio
static const int cs_radio = 13;
static const int radio_sleep = 26; //A2;
static const int irq_radio = 22;

// USB controller
static const int cs_32u4 = 18;
static const int irq_32u4 = 15;
#if defined(__AVR_ATmega1284P__)
// 32U4 IRQ is on pin change interrupt 3
#define BG_32U4_IRQ PCINT3_vect
// enable rising edge
#define BG_SD_READER_INTP() do \
  {                         \
    PCMSK3 |= _BV(PCINT31);  \
    PCICR |= _BV(PCIE3);    \
  }                         \
  while(0)                      
#endif

// SD card
static const int cs_sd = 12;
static const int sd_detect = 20;
static const int sd_pwr = 14;

// GPS on/off
static const int gps_on_off = 3;
#define bg_gps_pwr_config() pinMode(gps_on_off, OUTPUT)
#define bg_gps_on() digitalWrite(gps_on_off, LOW)
#define bg_gps_off() digitalWrite(gps_on_off, HIGH)
static const int gps_1pps = 25; // A1
// GPS 1PPS is on pin change interrupt 0
#define BG_1PPS_IRQ PCINT0_vect
// enable rising edge
#define BG_1PPS_INTP() do \
  {                         \
    PCMSK0 |= _BV(PCINT1);  \
    PCICR |= _BV(PCIE0);    \
  }                         \
  while(0)                      

// Geiger counter
static const int counts = 1;
static const int counts_int = 2;
static const int hvps_pwr = 27; // A3

// turn on and off high voltage power supply
#define bg_hvps_pwr_config() pinMode(hvps_pwr, OUTPUT)
#define bg_hvps_on() digitalWrite(hvps_pwr, LOW)
#define bg_hvps_off() digitalWrite(hvps_pwr, HIGH)

// LED on, off
static const int led = 4;
#define bg_led_config() pinMode(led, OUTPUT)
#define bg_led_on() digitalWrite(led, HIGH)
#define bg_led_off() digitalWrite(led, LOW)

#endif /* __PIN_DEFINITION_H__ */
