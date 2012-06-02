
#include <avr/interrupt.h>
#include <avr/io.h>

#define LED_init() DDRC |= (1 << DDC7)
#define LED_on() PORTC |= (1 << PORTC7)
#define LED_off() PORTC &= ~(1 << PORTC7)

#define configure_pin_irq() DDRB |= (1 << DDB7)
#define irq_low() PORTB &= ~(1 << PORTB7)
#define irq_high() PORTB |= (1 << PORTB7)

#define configure_pin_input() DDRD &= ~(1 << DDD0)
#define read_input() ((PORTD & _BV(PORTD0)) != 0)

uint8_t spi_rxtx_byte(uint8_t b)
{
  SPDR = b;
  irq_high();
  /* Wait for reception complete */
  while(!(SPSR & (1<<SPIF)))
    ;
  irq_low();
  /* Return Data Register */
  return SPDR;
}

void spi_init()
{
  // SS input
  DDRB &= ~(1 << DDB0);
  PORTB &= ~(1 << PORTB0);
  // SCK input
  DDRB &= ~(1 << DDB1);
  PORTB &= ~(1 << PORTB1);
  // MOSI input
  DDRB &= ~(1 << DDB2);
  PORTB &= ~(1 << PORTB2);
  // MISO output
  DDRB |= (1 << DDB3);

  // enable double-speed
  SPSR |= (1 << SPI2X);

  // enable SPI
  SPCR = (1 << SPE);
}

int main(void)
{
  uint32_t i = 0;
  uint8_t b = 1;

  LED_init();
  LED_off();

  spi_init();
  configure_pin_irq();
  irq_low();

	for (;;)
	{
    while (i < 100000)
      i++;
    i = 0;


    LED_on();
    spi_rxtx_byte(b++);
    LED_off();
/*
    if (i/20000 % 2 == 0)
    {
      LED_off();
      irq_low();
    }
    else
    {
      LED_on();
      SPDR = b++;
      //irq_high();
      //c = 0;
      //while (c++ < 128);
      // wait for byte to be shifted out
      while(!(SPSR & (1 << SPIF)));
      SPSR &= ~(1 << SPIF);
      //irq_low();
    }
    i++;
    */
  }
}
