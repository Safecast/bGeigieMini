#ifndef __SD_READER_INT_H__
#define __SD_READER_INT_H__

#include <Arduino.h>
#include <avr/interrupt.h> 
#include <bg3_pins.h>

/* 
 * Special instructions to set SPI speed and other
 * in main uC on bGeigie board
 */

// Main state variable
#define SD_READER_IDLE 0
#define SD_READER_ACTIVE 1
#define SD_READER_DISABLED 2
extern uint8_t sd_reader_state;

// buffer SIZE
#define SD_READER_BUF_SIZE 512

#define spi_delay() delayMicroseconds(20)

#define DEBUG 0
#if DEBUG
static const int break_pin = 16;
#define configure_break_pin() pinMode(break_pin, OUTPUT); \
                              digitalWrite(break_pin, LOW)
#define break_point() digitalWrite(break_pin, HIGH); \
                      delayMicroseconds(10); \
                      digitalWrite(break_pin, LOW)
#endif

#define select_32u4() digitalWrite(cs_32u4, LOW)
#define unselect_32u4() digitalWrite(cs_32u4, HIGH)

#define select_sd() digitalWrite(cs_sd, LOW)
#define unselect_sd() digitalWrite(cs_sd, HIGH)

#define sd_power_on() digitalWrite(sd_pwr, LOW)
#define sd_power_off() digitalWrite(sd_pwr, HIGH)

#define sd_card_present() (digitalRead(sd_detect) == 0)

// spi helpers
uint8_t spi_rx_byte();
void spi_tx_byte(uint8_t b);

// routine to call respectively in setup and loop
void sd_reader_setup();
void sd_reader_loop();

// initialize SD card
uint8_t sd_reader_init();

// lock/unlock SD reader
uint8_t sd_reader_lock();
void sd_reader_unlock();

// basic raw SD operations
uint8_t sd_reader_read_block(uint32_t arg);
uint8_t sd_reader_write_block(uint32_t arg);
void sd_reader_get_info();

// used to receive data after interrupt from 32u4
extern int sd_reader_interrupted;
void sd_reader_process_interrupt();

#endif /* __SD_READER_INT_H__ */
