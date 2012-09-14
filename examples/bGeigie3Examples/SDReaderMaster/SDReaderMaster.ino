
#include <SPI.h>
#include <SD.h>
#include <sd_reader_int.h>
#include <avr/interrupt.h>
#include <bg3_pins.h>

void setup()
{
  Serial.begin(57600);
  Serial.println("Welcome to SD Reader.");

  // setup sd reader
  sd_reader_setup();

  // setup led
  bg_led_config();
  bg_led_off();
}

void loop()
{
  //if (millis() % 2000 < 1000)
    //digitalWrite(4, LOW);
  //else
    //digitalWrite(4, HIGH);
  sd_reader_loop();
}

