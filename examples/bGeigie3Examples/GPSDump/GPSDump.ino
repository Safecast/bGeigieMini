/*
  Blink
  Turns on an LED on for one second, then off for one second, repeatedly.
 
  This example code is in the public domain.
 */
 
 #include <SD.h>
 #include <SPI.h>
 #include <bg3_pins.h>
 #include <GPS.h>
 
void setup() {                
  Serial.begin(57600);   // to computer
  Serial1.begin(9600);  // from GPS

  pinMode(gps_on_off, OUTPUT);
  digitalWrite(gps_on_off, LOW);

  Serial.println("Wait for GPS to start.");
  int t = 0;
  while(!Serial1.available())
  {
    delay(10);
    t++;
  }
  Serial.print("GPS started in less than ");
  Serial.print(t*10);
  Serial.println("ms.");

  // Example of how to issue commands to the GPS module
  Serial1.println(MTK_SET_NMEA_OUTPUT_ALLDATA);
  Serial1.println(MTK_UPDATE_RATE_1HZ);

  // initialize GPS 1PPS input
  pinMode(gps_1pps, INPUT);

  // initilize led
  bg_led_config();
  bg_led_off();
  
}

void loop() {
  // read from serial if available
  if (Serial1.available())
  {
    char c = Serial1.read();
    Serial.print(c);
  }
  // output 1PPS on led
  digitalWrite(led, digitalRead(gps_1pps));
}
