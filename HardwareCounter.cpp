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

#include "HardwareCounter.h"

// Constructor
HardwareCounter::HardwareCounter(long delay)
{
  _delay = delay;
}

// call this to start the counter
void HardwareCounter::start()
{

  // hardware counter setup ( refer atmega168.pdf chapter 16-bit counter1)
  TCCRnA=0;     // reset timer/countern control register A
  TCCRnB=0;     // reset timer/countern control register A
  TCNTn=0;      // counter value = 0
  // set timer/counter1 hardware as counter , counts events on pin Tn ( arduino pin 5 on 168, pin 47 on Mega )
  // normal mode, wgm10 .. wgm13 = 0
  sbi (TCCRnB ,CS10);  // External clock source on Tn pin. Clock on rising edge.
  sbi (TCCRnB ,CS11);
  sbi (TCCRnB ,CS12);
  TCCRnB = TCCRnB | 7;  //  Counter Clock source = pin Tn , start counting now

  // set start time
  _start_time = millis();

  // set count to zero (optional)
  _count = 0;
}

// call this to read the current count and save it
unsigned int HardwareCounter::count()
{

  TCCRnB = TCCRnB & ~7;   // Gate Off  / Counter Tn stopped
  _count = TCNTn;         // Set the count in object variable
  TCCRnB = TCCRnB | 7;    // restart counting
  return _count;

}

// This indicates when the count over the determined period is over
int HardwareCounter::available()
{
  return (mills() - _start_time >= _delay)
}


