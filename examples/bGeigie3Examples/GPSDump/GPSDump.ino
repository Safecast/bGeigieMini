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
  // initialize the digital pin as an output.
  // Pin 13 has an LED connected on most Arduino boards:  
  Serial.begin(9600);   // to computer
  Serial1.begin(9600);  // from GPS

  pinMode(gps_on_off, OUTPUT);
  digitalWrite(gps_pwr, LOW);

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

  // Issue some commands to the GPS
  Serial1.println(MTK_SET_NMEA_OUTPUT_RMCGGA);
  Serial1.println(MTK_UPDATE_RATE_1HZ);
  
}

void loop() {
  while (Serial1.available())
  {
    char c = Serial1.read();
    Serial.print(c);
  }
}
