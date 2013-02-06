
#ifndef __LEDS_H__
#define __LEDS_H__

/* Includes: */
#include <avr/io.h>

  /* define LED macro */
#define LED_init() DDRC |= (1 << DDC7)
#define LED_on() PORTC |= (1 << PORTC7)
#define LED_off() PORTC &= ~(1 << PORTC7)

#endif
