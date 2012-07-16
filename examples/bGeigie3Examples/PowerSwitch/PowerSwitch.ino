
#include <SD.h>
#include <SPI.h>

#include <avr/sleep.h>

#include <bg3_pins.h>
#include <bg_pwr.h>

// on wake up and sleep routine
void wakeup();
void sleepy();

void setup()
{
  Serial.begin(57600);
  bg_led_config();
  bg_led_off();

  pinMode(main_switch, INPUT);
  Serial.println(digitalRead(main_switch));

  // enable interrupt
  sei();

  // call that last in setup because it will sleep the board.
  bg_pwr_init(main_switch, wakeup, sleepy);
}

void loop()
{
  // power management loop routine
  bg_pwr_loop();

  // task (blink LED)
  bg_led_on();
  delay(500);
  bg_led_off();
  delay(500);
}

void wakeup()
{
  Serial.print("Wake up! state=");
  Serial.println(bg_pwr_state);
}

void sleepy()
{
  bg_led_off();
  Serial.print("Sleeping now... state=");
  Serial.println(bg_pwr_state);
  delay(10);
}

