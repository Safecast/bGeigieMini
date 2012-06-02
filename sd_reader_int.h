#ifndef __SD_READER_INT_H__
#define __SD_READER_INT_H__

#include <Arduino.h>
#include <avr/interrupt.h> 

// IRQ is on pin change interrupt 2
#define BGEIGIE_32U4_IRQ PCINT2_vect
// enable rising edge
#define CFG_SD_READER_INTP() do \
  {                         \
    PCMSK2 |= _BV(PCINT23);  \
    PCICR |= _BV(PCIE2);    \
  }                         \
  while(0)                      

/* 
 * Special instructions to set SPI speed and other
 * in main uC on bGeigie board
 */

// Main state variable
#define IDLE 0
#define SD_READER 1
static uint8_t state = IDLE;

// buffer SIZE
#define SD_READER_BUF_SIZE 256

// IRQ is pin 23 (PORTC7)
static const int cs_32u4 = 0;
static const int irq_32u4 = 23;
static const int cs_sd = 12;
static const int sd_pwr = 14;
static const int sd_detect = 20;
static const int led = 13;

#define select_32u4() digitalWrite(cs_32u4, LOW)
#define unselect_32u4() digitalWrite(cs_32u4, HIGH)

#define select_sd() digitalWrite(cs_sd, LOW)
#define unselect_sd() digitalWrite(cs_sd, HIGH)

#define sd_power_on() digitalWrite(sd_pwr, LOW)
#define sd_power_off() digitalWrite(sd_pwr, HIGH)

#define sd_card_present() (digitalRead(sd_detect) == 0)

uint8_t spi_rx_byte();
void spi_tx_byte(uint8_t b);
void sd_reader_setup();
void sd_reader_loop();
uint8_t sd_reader_init();
uint8_t sd_reader_read_block(uint32_t arg);
uint8_t sd_reader_write_block(uint32_t arg);

#endif /* __SD_READER_INT_H__ */
