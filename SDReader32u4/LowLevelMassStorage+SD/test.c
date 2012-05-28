
#include <avr/io.h>

#define LED_init() DDRC |= (1 << DDC7)
#define LED_on() PORTC |= (1 << PORTC7)
#define LED_off() PORTC &= ~(1 << PORTC7)

#define configure_pin_irq() DDRB |= (1 << DDB7)
#define irq_low() PORTB &= ~(1 << PORTB7)
#define irq_high() PORTB |= (1 << PORTB7)

#define configure_pin_input() DDRD &= ~(1 << DDD0)
#define read_input() ((PORTD & _BV(PORTD0)) != 0)

int main(void)
{
  uint32_t i = 0;
  LED_init();
  configure_pin_irq();
  //LED_on();

  //configure_pin_input();

	while (1)
	{
    if (i/20000 % 2 == 0)
    {
      LED_on();
      irq_low();
    }
    else
    {
      LED_off();
      irq_high();
    }
    i++;
    //if (PORTD & _BV(PORTD0))
      //LED_on();
    //else
      //LED_off();
  }
}
