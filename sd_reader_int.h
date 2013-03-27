/*
   This code allows the SD card to be read from a different CPU through the
   SPI interface. The cpu on this side is the master and the external CPU
   is the slave. The communication is interrupt request based.

   Copyright (c) 2011, Robin Scheibler aka FakuFaku
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the <organization> nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

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
int sd_reader_setup();
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

#ifdef SOFT_RESET
// reset the CPU
void cpu_reset();
#endif

#endif /* __SD_READER_INT_H__ */
