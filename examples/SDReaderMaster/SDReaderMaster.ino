
#include <SPI.h>
#include <SD.h>
#include <sd_reader_int.h>
#include <avr/interrupt.h>

// Sketch to use the bGeigie as Mass storage
// Still need to implememnt timer to deactivate
// after some timeout (e.g. 2 min)

int seconds;

void setup()
{
  sei();
  Serial.begin(57600);
  Serial.println("Welcome to SD Reader.");
  // setup sd reader
  sd_reader_setup();
  Serial.print("IRQ: ");
  Serial.println(digitalRead(irq_32u4));
  seconds = 0;

}

void loop()
{
  sd_reader_loop();
  /*
  unsigned int m = millis()/1000;
  if (m > seconds)
  {
    Serial.print("Tick: IRQ=");
    Serial.println(digitalRead(irq_32u4));
    seconds = m;
  }
  */
  digitalWrite(led, digitalRead(irq_32u4));
}

