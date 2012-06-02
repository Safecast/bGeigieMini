
#include <SPI.h>
#include <SD.h>
#include "sd_reader_int.h"

// SD card object
Sd2Card card;

// 512 block buffer to communicate with SD card
uint8_t buffer[SD_READER_BUF_SIZE];

// Timeout is 30s
unsigned long last_interrupt;
#define SD_READER_TIMEOUT 30000


// We only need to know these two commands
/* CMD17: arg0[31:0]: data address, response R1 */
#define CMD_READ_SINGLE_BLOCK 0x11
/* CMD24: arg0[31:0]: data address, response R1 */
#define CMD_WRITE_SINGLE_BLOCK 0x18

void sd_reader_setup()
{
  // initlalize all the pins
  pinMode(cs_32u4, OUTPUT);
  unselect_32u4();    // unselect

  pinMode(cs_sd, OUTPUT);
  unselect_sd();

  pinMode(sd_pwr, OUTPUT);
  sd_power_off();

  // configure SPI pins
  pinMode(MISO, INPUT);
  pinMode(MOSI, OUTPUT);
  pinMode(SCK, OUTPUT);
  // make sure that SS pin is output to be SPI master
  pinMode(SS, OUTPUT);

  // configure SPI
  //SPI.setBitOrder(MSBFIRST);
  //SPI.setDataMode(SPI_MODE0);
  //SPI.setClockDivider(SPI_CLOCK_DIV2);
  SPI.begin();

  pinMode(sd_detect, INPUT);
  digitalWrite(sd_detect, HIGH);

  pinMode(led, OUTPUT);     // LED
  digitalWrite(led, LOW);  // turn LED on

  pinMode(irq_32u4, INPUT); // IRQ
  CFG_SD_READER_INTP();     // set pin change interrupt

  // initialize buffer to zero
  for (int i = 0 ; i < SD_READER_BUF_SIZE ; i++)
    buffer[i] = 0x0;

  // deblock the SPI if IRQ is high
  if (digitalRead(irq_32u4))
  {
    uint8_t b;
    Serial.print("Unblock SPI: ");
    select_32u4();
    delay(10);
    b = SPI.transfer(0x0);
    delay(5);
    unselect_32u4();
    Serial.println(b, HEX);
  }
}

void sd_reader_loop()
{
  unsigned long now = millis();
  if (state == SD_READER && now - last_interrupt > SD_READER_TIMEOUT)
  {
    Serial.println("SD reader timeout. Turning off.");
    sd_power_off();
    state = IDLE;
  }
}

uint8_t sd_reader_init()
{
  uint8_t status;

  // if card not inserted, return false
  if (!sd_card_present())
    return 0;

  // power on sd card
  sd_power_on();

  // initialize card (starts SPI as well)
  status = card.init(SPI_FULL_SPEED, cs_sd);

  if (status)
    state = SD_READER;
    
  return status;
}

uint8_t spi_rx_byte()
{
  return SPI.transfer(0x0);
}

void spi_tx_byte(uint8_t b)
{
  SPI.transfer(b);
}

ISR(BGEIGIE_32U4_IRQ)
{
  uint8_t cmd = 0;
  uint32_t arg = 0;

  cli(); // disable interrupt

  // we need to check the pin direction
  //int pinval = PINC & _BV(PINC7);
  int pinval = digitalRead(irq_32u4);

  Serial.print("Interrupted! IRQ=");
  Serial.println(pinval);

  if (pinval)
  {
    select_32u4();
    uint8_t b = spi_rx_byte();
    delay(10);
/*
    for (int i = 0 ; i < SD_READER_BUF_SIZE ; i++)
    {
      buffer[i] = spi_rx_byte();
      delay(10);
    }
    */
    unselect_32u4();

    Serial.print("Received bytes: ");
    Serial.println(b);
    /*
    for (int i = 0 ; i < SD_READER_BUF_SIZE ; i++)
      Serial.println(buffer[i]);
      */

/*
    // light LED off
    digitalWrite(led, HIGH);
    Serial.println("Hello from interrupt!");

    // update timestamp
    last_interrupt = millis();

    // get command from 32u4
    select_32u4();
    cmd = spi_rx_byte();
    arg |= ((uint32_t)spi_rx_byte() << 24);
    arg |= ((uint32_t)spi_rx_byte() << 16);
    arg |= ((uint32_t)spi_rx_byte() << 8);
    arg |= ((uint32_t)spi_rx_byte() << 0);
    spi_rx_byte(); // closing byte
    unselect_32u4();

    Serial.print("Command: ");
    Serial.println(cmd);
    Serial.print("Argument: ");
    Serial.println(arg);

    // initialize if necessary
    if (state == IDLE)
    {
      if (sd_reader_init())
      {
        Serial.println("Initialized SD card.");
        state = SD_READER;
      }
      else
      {
        Serial.println("Failed SD reader initialization.");
        // if we fail to init, return error and quit
        select_32u4();
        spi_tx_byte(0xff);
        unselect_32u4();
        return;
      }
    }

    // action time!
    switch (cmd)
    {
      case CMD_READ_SINGLE_BLOCK:
        Serial.print("R ");
        Serial.println(arg);
        sd_reader_read_block(arg);
        break;

      case CMD_WRITE_SINGLE_BLOCK:
        Serial.print("W ");
        Serial.println(arg);
        sd_reader_write_block(arg);
        break;
      
      default:
        // return fail value
        Serial.print("Unknown: ");
        Serial.println(arg);
        select_32u4();
        spi_tx_byte(0xff);
        unselect_32u4();
        break;
    }

    // light LED on
    digitalWrite(led, LOW);
  */
  }

  sei(); // enable interrupt

}

uint8_t sd_reader_read_block(uint32_t arg)
{
  // read block from card
  if (!card.readBlock(arg, buffer))
  {
    // failure
    return 0;
  }

  // send back to 32u4
  select_32u4();

  spi_tx_byte(0x0);   // signal success

  spi_tx_byte(0xfe);  // magic number

  for (uint16_t i = 0 ; i < 512 ; i++) // send block
    spi_tx_byte(buffer[i]);

  spi_tx_byte(0xff);   // dummy data for CRC
  spi_tx_byte(0xff);   // dummy data for CRC

  unselect_32u4();

  return 1;

}

uint8_t sd_reader_write_block(uint32_t arg)
{
  // send back to 32u4
  select_32u4();

  spi_tx_byte(0x0);   // signal success

  while (spi_rx_byte() != 0xfe);  // wait beginning of transfer

  for (uint16_t i = 0 ; i < 512 ; i++) // send block
    buffer[i] = spi_rx_byte();

  spi_rx_byte();   // receive dummy CRC
  spi_rx_byte();   // receive dummy CRC

  unselect_32u4();

  // write block from card
  if (!card.writeBlock(arg, buffer))
    return 0;
  else
    return 1;

}


