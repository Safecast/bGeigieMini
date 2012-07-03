/*
  Blink
  Turns on an LED on for one second, then off for one second, repeatedly.
 
  This example code is in the public domain.
 */
 
 // needed to compile library correctly
 #include <SD.h>

 #include <GPS.h>

 // all GPS command taken from datasheet
 // "Binary Messages Of SkyTraq Venus 6 GPS Receiver"

 static const uint8_t GPS_MSG_FW[2] PROGMEM = { 0x02, 0x00 };
 #define GPS_MSG_FW_L 2

 static const uint8_t GPS_MSG_FACTORY_DEFAULT[2] PROGMEM = { 0x04, 0x01 }; // with reboot after reset
 #define GPS_MSG_FACTORY_DEFAULT_L 2

 static const uint8_t GPS_MSG_OUTPUT_GGARMC_1S[9] PROGMEM = { 0x08, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01 }; // with update to RAM and FLASH
 #define GPS_MSG_OUTPUT_GGARMC_1S_L 9

 static const uint8_t GPS_MSG_OUTPUT_GGARMC_2S[9] PROGMEM = { 0x08, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x01 }; // with update to RAM and FLASH
 #define GPS_MSG_OUTPUT_GGARMC_2S_L 9

 static const uint8_t GPS_MSG_PWR_SAVE[3] PROGMEM = { 0x0C, 0x01, 0x01 }; // update to FLASH too
 #define GPS_MSG_PWR_SAVE_L 3

 static const uint8_t GPS_MSG_PWR_NORMAL[3] PROGMEM = { 0x0C, 0x00, 0x01 }; // update to FLASH too
 #define GPS_MSG_PWR_NORMAL_L 3

 
void setup() {                
  // initialize the digital pin as an output.
  // Pin 13 has an LED connected on most Arduino boards:  
  Serial.begin(9600);   // to computer

  Serial.println("Wait for GPS to start.");
  int t = 0;
  while(!Serial.available())
  {
    delay(10);
    t++;
  }
  Serial.print("GPS started in less than ");
  Serial.print(t*10);
  Serial.println("ms.");


  // Issue some commands to the GPS
  gps_send_message(GPS_MSG_FW, GPS_MSG_FW_L);
  gps_send_message(GPS_MSG_OUTPUT_GGARMC_2S, GPS_MSG_OUTPUT_GGARMC_2S_L);
  gps_send_message(GPS_MSG_PWR_SAVE, GPS_MSG_PWR_SAVE_L);
  
}

void loop() {
  while (Serial.available())
  {
    char c = Serial.read();
    Serial.print(c);
  }
}

void gps_send_message(const uint8_t *msg, uint16_t len)
{
  uint8_t chk = 0x0;
  // header
  Serial.print(0xA0, BYTE);
  Serial.print(0xA1, BYTE);
  // send length
  Serial.print(len >> 8, BYTE);
  Serial.print(len & 0xff, BYTE);
  // send message
  for (int i = 0 ; i < len ; i++)
  {
    Serial.print(msg[i], BYTE);
    chk ^= msg[i];
  }
  // checksum
  Serial.print(chk, BYTE);
  // end of message
  Serial.print(0x0D, BYTE);
  Serial.println(0x0A, BYTE);
}
