
#ifndef __TIMER_H__
#define __TIMER_H__

/* Simple milliseconds counter */

void timer_init(void);
unsigned long millis(void);
void delay(unsigned long ms);

#endif /* __TIMER_H__ */
