/*
   HardwareCounter.ino
   Example of using the Hardware Counter library

   This example will compute the one minute number of counts using a 12 bins
   sliding window, updating the value every 5 seconds.  Obviously, one has to
   wait one full minute before having a correct value for the last minute.

   Created 2011, Robin Scheibler aka FakuFaku
   This example is in the public domain.
*/

#include <HardwareCounter.h>

#define DELAY 5000
#define NBINS 12
#define TIMER1 5    // the timer1 pin on the 328p

static HardwareCounter counter(TIMER1, DELAY);  // The hardware counter object
static unsigned long bins[NBINS] = {0};         // the sliding window
static int index = 0;                           // the current bin index

void setup()
{

  // start serial
  Serial.begin(9600);

  // output some nice message
  Serial.println("Start counting counts!");

  // Start the counter
  counter.start();

}

void loop()
{
  // if counter result available
  if (counter.available())
  {
    int cpb = counter.count();    // fetch value of counter
    int i;
    int cpm = 0;
    bins[index] = cpb;
    for (i = 0 ; i < NBINS ; i++) // compute sum of all bins
      cpm += bins[i];
    index = (index+1)%NBINS;      // update bin index

    // print out result
    Serial.print("Counts in last 5 seconds: ");
    Serial.print(cpb);
    Serial.print(" and last minute: ");
    Serial.println(cpm);

    // restart the counter
    counter.start();
  }
}

