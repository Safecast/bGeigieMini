
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

uint8_t spi_rxtx_byte(uint8_t b);
uint8_t spi_rx_byte(void);
void spi_tx_byte(uint8_t b);
uint8_t spi_irq(uint8_t b);
void spi_init(void);
void delay_loop(uint32_t d);
uint8_t buffer[256];

int main(void)
{
  int i = 0;

  while (i < 256)
  {
    buffer[i] = (uint8_t)(255 - i);
    i++;
  }

  LED_init();
  LED_off();

  spi_init();

  configure_pin_irq();
  irq_low();

  LED_on();
  delay_loop(200000);
  LED_off();
  delay_loop(200000);
  LED_on();
  delay_loop(200000);
  LED_off();
  delay_loop(200000);
  LED_on();

	for (;;)
	{
    uint8_t byte = 0x0;
    delay_loop(100000);
    LED_off();
    delay_loop(100000);
    LED_on();

    // Interrupt ReQuest to 1284p
    irq_high();

    // wait for SS to go low
    //while (!(PORTB & (1 << PORTB0)));

    // initiate
    byte = spi_rxtx_byte(0xff);

    // send bytes
    for (i = 0 ; i < 256 ; i++)
      spi_tx_byte(buffer[i]);

    irq_low();

    delay_loop(1000);
    //spi_rxtx(0x01);
    LED_off();

  }
}

uint8_t spi_rxtx_byte(uint8_t b)
{
  SPDR = b;
  /* Wait for reception complete */
  while(!(SPSR & (1<<SPIF)))
    ;
  /* Return Data Register */
  return SPDR;
}

uint8_t spi_rx_byte(void)
{
  SPDR = 0x0;
  /* Wait for reception complete */
  while(!(SPSR & (1<<SPIF)))
    ;
  /* Return Data Register */
  return SPDR;
}

void spi_tx_byte(uint8_t b)
{
  SPDR = b;
  /* Wait for reception complete */
  while(!(SPSR & (1<<SPIF)))
    ;
  /* Return Data Register */
  return;
}

uint8_t spi_irq(uint8_t b)
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

void spi_init(void)
{
  uint8_t b;
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
  // NOT WORKING YET
  //SPSR |= (1 << SPI2X);

  // enable SPI
  SPCR = (1 << SPE);

  // reading these two register should start spi
  b = SPDR;
  b = SPSR;

  return;
}

void delay_loop(uint32_t d)
{
  uint32_t i = 0x0;
  for (i = 0x0 ; i < d ; i++);
  return;
}

