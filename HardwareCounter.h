/*
   Simple library for Arduino implementing a hardware counter
   for a Geigier counter for example

   Copyright (C) 2011 Robin Scheibler aka FakuFaku

   Permission is hereby granted, free of charge, to any person obtaining a copy of
   this software and associated documentation files (the "Software"), to deal in
   the Software without restriction, including without limitation the rights to
   use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
   of the Software, and to permit persons to whom the Software is furnished to do
   so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/

#ifndef COUNTER_H
#define COUNTER_H

// Link to arduino library
#if ARDUINO >= 100
#include <Arduino.h>
#else
#include <WProgram.h>
#endif

// set bit macro
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

#if defined(__AVR_ATmega1280__)
#define TCCRnA TCCR5A
#define TCCRnB TCCR5B
#define TCNTn  TCNT5
#define TIFRn  TIFR5
#define TOVn   TOV5
#else
#define TCCRnA TCCR1A
#define TCCRnB TCCR1B
#define TCNTn  TCNT1
#define TIFRn  TIFR1
#define TOVn   TOV1
#endif

// Defining the Class for the counter
Class HardwareCounter
{
  // public
  public:
    void HardwareCounter(long delay);
    void start();
    int available();
    unsigned int count();

  // privatee
  private:
    long _start_time;
    long _delay;
    unsigned int _count;

}

#endif /* COUNTER_H */
