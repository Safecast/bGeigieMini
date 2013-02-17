
#ifndef __LEDS_H__
#define __LEDS_H__

/* Includes: */
#include <avr/io.h>

  /* define LED macro */
#define LED_init()  DDRF  |=  (1 << DDF0)
#define LED_on()    PORTF |=  (1 << PORTF0)
#define LED_off()   PORTF &= ~(1 << PORTF0)

#endif
