
#include <SD.h>
#include <SPI.h>

#include <stdio.h>

#include <bg3_pins.h>
#include <bg_sensors.h>
#include <sd_logger.h>

char filename[] = "sensors.log";

void setup()
{
  Serial.begin(57600);

  // turn off bits of hardware not used
  pinMode(SS, OUTPUT);
  digitalWrite(SS, HIGH);
  pinMode(hvps_pwr, INPUT);
  pinMode(gps_on_off, INPUT);
  pinMode(radio_sleep, INPUT);
  pinMode(cs_radio, OUTPUT);
  digitalWrite(cs_radio, HIGH);
  pinMode(cs_32u4, OUTPUT);
  digitalWrite(cs_32u4, HIGH);

  // configure LED
  bg_led_config();

  // init sensors
  bgs_sensors_init(sense_pwr, batt_sense, temp_sense, hum_sense, hv_sense);

  // init SD card
  sd_log_init(sd_pwr, sd_detect, cs_sd);
}

void loop()
{
  char line[50];
  int b, v;
  float t, h;
  int tf, hf;

  bg_sensors_on();
  delay(100); // wait for value to stabilize

  b = 1000*bgs_read_battery();
  v = (int)bgs_read_hv();

  // need to compute fractional part separately because sprintf doesn't support it
  t = bgs_read_temperature();
  tf = (int)((t-(int)t)*100);
  h = bgs_read_humidity();
  hf = (int)((h-(int)h)*100);

  memset(line, 0, 50);
  sprintf(line, "%dmV,%dV,%d.%dC,%d.%d", b, v, (int)t, tf, (int)h, hf);

  bg_sensors_off();

  Serial.println(line);

  // write new line to log file on SD card
  sd_log_writeln(filename, line);

  bg_led_on();
  delay(2000);
  bg_led_off();
  delay(2000);
}

