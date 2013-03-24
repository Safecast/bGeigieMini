
#ifndef __TIMER_H__
#define __TIMER_H__

/**
 * (c) 2013  Robin Scheibler, Safecast (fakufaku [at] gmail [dot] com)
 * Code copied mostly from arduino/wiring.c
 */

/* Simple milliseconds counter */

void timer_init(void);
unsigned long millis(void);
void delay(unsigned long ms);

#endif /* __TIMER_H__ */
