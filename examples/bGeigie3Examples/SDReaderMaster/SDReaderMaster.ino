
#include <SPI.h>
#include <SD.h>
#include <sd_reader_int.h>
#include <avr/interrupt.h>

void setup()
{
  sei();
  Serial.begin(57600);
  Serial.println("Welcome to SD Reader.");
  // setup sd reader
  sd_reader_setup();
  Serial.print("IRQ: ");
  Serial.println(digitalRead(irq_32u4));

}

void loop()
{
  sd_reader_loop();
}

