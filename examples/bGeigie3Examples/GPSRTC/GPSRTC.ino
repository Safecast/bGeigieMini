
#include <SD.h>
#include <SPI.h>

#include <bg3_pins.h>
#include <GPS.h>

// the line buffer for serial receive and send
static char line[LINE_SZ];

int rtc_pulse = 0;

void setup()
{
  Serial.begin(57600);
  bg_led_config();
  bg_led_off();

  // initialize GPS using second Serial connection
  Serial1.begin(9600);
  gps_init(&Serial1, line);
  bg_gps_pwr_config();
  bg_gps_on();

  // Initialize interrupt on GPS 1PPS line
  pinMode(gps_1pps, INPUT);
  BG_1PPS_INTP();

  sei();
}

void loop()
{
  //digitalWrite(led, digitalRead(gps_1pps));
  if (rtc_pulse == 5)
  {
    rtc_pulse = 0;
    bg_led_on(); 
    delay(1000);
    bg_led_off();
  }
}

ISR(BG_1PPS_INT)
{
  int val = *portInputRegister(digitalPinToPort(gps_1pps)) & digitalPinToBitMask(gps_1pps);
  if (val)
  {
    // make good use of GPS 1PPS here
    rtc_pulse++;
  }
}
