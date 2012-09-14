
#include <avr/interrupt.h>

#include <SPI.h>
#include <SD.h>

#include <bg3_pins.h>
#include <bg_pwr.h>
#include <sd_reader_int.h>


void setup()
{
  Serial.begin(57600);
  Serial.println("Welcome to SD Reader.");

  // setup sd reader
  sd_reader_setup();

  // init power management
  bg_pwr_init(main_switch, power_up, shutdown);

  // config LED *after* SD card init because it drives LED high
  bg_led_config();
  bg_led_off();
}

void loop()
{
  bg_pwr_loop();

  sd_reader_loop();

  // execute task only if running and not in busy SD reader mode
  if (bg_pwr_running() && sd_reader_lock())
  {
    if (millis() % 2000 < 1000)
      bg_led_on();
    else
      bg_led_off();
  }
  // unlock SD reader
  sd_reader_unlock();
}

void power_up()
{
  Serial.println("Power up!");
  blink_led(2, 100);
}

void shutdown()
{
  Serial.println("Shutdown!");
  sd_power_off();
  blink_led(3, 100);
}

void blink_led(unsigned int N, unsigned int D)
{
  for (int i=0 ; i < N ; i++)
  {
    bg_led_on();
    delay(D);
    bg_led_off();
    delay(D);
  }
}
