
// Link to arduino library
/*
#if ARDUINO >= 100
#include <Arduino.h>
#else
#include <WProgram.h>
#endif
*/

// Link to arduino library
#if ARDUINO >= 100
#include <Arduino.h>
#else
#include <WProgram.h>
#endif

#include <SD.h>
#include <avr/io.h>
#include "sd_logger.h"
#include "sd_reader_int.h"

// Various messages that we store in flash
#define BG_ERR_MSG_SIZE 40
static char msg_sd_not_present[] PROGMEM = "SD not inserted.\n";
static char msg_sd_init[] PROGMEM = "SD init...\n";
static char msg_card_failure[] PROGMEM = "Card failure...\n";
static char msg_card_ready[] PROGMEM = "SD card ready\n";
static char msg_error_log_file_cannot_open[] PROGMEM = "Error: Log file cannot be opened.\n";

/* sd card power */
int sd_log_pwr;

/* sd card detect */
int sd_log_detect;
int sd_log_initialized;
int sd_log_inserted;
int sd_log_last_write;

/* sd chip select pin */
static int sd_log_cs;

/* the FILE */
File dataFile;

// Initialize SD card
int sd_log_init(int pin_pwr, int pin_detect, int pin_cs)
{
  char tmp[BG_ERR_MSG_SIZE];

  // initialize pins
  sd_log_pwr = pin_pwr;
  pinMode(sd_log_pwr, OUTPUT);
  sd_log_pwr_off();

  sd_log_detect = pin_detect;
  pinMode(sd_log_detect, INPUT);
  digitalWrite(sd_log_detect, HIGH);  // activate pull-up

  sd_log_cs = pin_cs;
  pinMode(sd_log_cs, OUTPUT);
  digitalWrite(sd_log_cs, HIGH);

  // initialize status variables
  sd_log_initialized = 0;
  sd_log_inserted = 0;
  sd_log_last_write = 1; // because we assume things are working when we start

  // check if Card is inserted
  if (sd_log_card_missing())
  {
    strcpy_P(tmp, msg_sd_not_present);
    Serial.print(tmp);
    return 0;
  }

  // set card to inserted
  sd_log_inserted = 1;

  // turn on SD card
  sd_log_pwr_on();

  // see if the card can be initialized:
  if (!SD.begin(sd_log_cs)) {
    strcpy_P(tmp, msg_card_failure);
    Serial.print(tmp);
    return 0;
  }

  // set card as initialized
  sd_log_initialized = 1;

  // output success
  strcpy_P(tmp, msg_card_ready);
  Serial.print(tmp);

  // return success
  return 1;
}

// write a line to a file in SD card
int sd_log_writeln(char *filename, char *log_line)
{
  // if SD reader mode is not idle, fail
  //if (sd_reader_state != SD_READER_IDLE)
  //{
    //sd_log_last_write = 0;
    //return 0;
  //}

  // test for card presence
  if (sd_log_card_missing())
  {
    char tmp[BG_ERR_MSG_SIZE];
    strcpy_P(tmp, msg_sd_not_present);
    Serial.print(tmp);
    sd_log_last_write = 0;
    sd_log_inserted = 0;
    return 0;
  }

  // start critical bit
  uint8_t sreg_old = SREG;  // save sreg
  cli();                    // disable global interrupts

  // open file
  dataFile = SD.open(filename, FILE_WRITE);
  if (dataFile)
  {
    dataFile.print(log_line);
    dataFile.print("\n");
    dataFile.close();
    sd_log_last_write = 1;
  }
  else
  {
    char tmp[BG_ERR_MSG_SIZE];
    strcpy_P(tmp, msg_error_log_file_cannot_open);
    Serial.print(tmp);
    sd_log_last_write = 0;
  }   

  // re-enable interrupts
  SREG = sreg_old;

  return sd_log_last_write;
}

