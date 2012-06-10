
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

/* Custom commands to turn on and off SD card reader */
#define CMD_SD_ON 0x3c
#define CMD_SD_OFF 0x3d

void sd_reader_setup()
{
#if DEBUG
  configure_break_pin();
#endif

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
  SPI.setClockDivider(SPI_CLOCK_DIV4);  // 2X speed not working now
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

  // initilize SD card
  if (sd_reader_init())
    Serial.println("SD initialized.");
  else
    Serial.println("SD init failed.");

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
  status = card.init(SPI_HALF_SPEED, cs_sd);

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
  uint8_t ret = 0;

  uint8_t sreg_old = SREG;
  cli(); // disable interrupt

  // we need to check the pin direction
  //int pinval = PINC & _BV(PINC7);
  int pinval = digitalRead(irq_32u4);

  //Serial.print("IRQ=");
  //Serial.println(pinval);

  if (pinval)
  {

/* THIS CODE HERE WORKS
    select_32u4();
    //delay(1);
    uint8_t b = spi_rx_byte();

    //delay(10);

    for (int i = 0 ; i < SD_READER_BUF_SIZE ; i++)
    {
      delayMicroseconds(10);
      buffer[i] = spi_rx_byte();
    }

    unselect_32u4();
 */

    // light LED off
    digitalWrite(led, HIGH);

    // update timestamp
    last_interrupt = millis();

    // get command from 32u4
    // This is 5 SPI transfers
    select_32u4();
    cmd = spi_rx_byte();
    spi_delay();
    arg |= ((uint32_t)spi_rx_byte() << 24);
    spi_delay();
    arg |= ((uint32_t)spi_rx_byte() << 16);
    spi_delay();
    arg |= ((uint32_t)spi_rx_byte() << 8);
    spi_delay();
    arg |= ((uint32_t)spi_rx_byte() << 0);
    spi_delay();
    unselect_32u4();

    // action time!
    switch (cmd)
    {
      case 0x40 | CMD_SD_ON:
        ret = 0x0;
        // initialize SD card
        /*
        if (!sd_reader_init())
          ret = 0xff; // fail value
          */
        // send return value
        select_32u4();
        spi_tx_byte(ret);
        unselect_32u4();
        break;

      case 0x40 | CMD_SD_OFF:
        // turn SD card off
        sd_power_off();
        state = IDLE;
        // send response
        select_32u4();
        spi_tx_byte(0x0);
        unselect_32u4();
        break;

      case 0x40 | CMD_READ_SINGLE_BLOCK:
        sd_reader_read_block(arg);
        break;

      case 0x40 | CMD_WRITE_SINGLE_BLOCK:
        sd_reader_write_block(arg);
        break;
      
      default:
        // return fail value
        select_32u4();
        spi_tx_byte(0xff);
        unselect_32u4();
        break;
    }

    // light LED on
    digitalWrite(led, LOW);
  }

  SREG = sreg_old; // enable interrupt

}

uint8_t sd_reader_read_block(uint32_t arg)
{
  // read block from card
  if (!card.readBlock(arg, buffer))
  {
    // failure
    select_32u4();
    spi_tx_byte(0xff);   // signal failure
    unselect_32u4();
    Serial.println("Failed to read block.");
    return 0;
  }

  // send back to 32u4
  select_32u4();

  spi_tx_byte(0x0);   // signal success
  spi_delay();

  spi_tx_byte(0xfe);  // magic number
  spi_delay();

  for (uint16_t i = 0 ; i < 512 ; i++) // send block
  {
    spi_tx_byte(buffer[i]);
    spi_delay();
  }

  spi_tx_byte(0xff);   // dummy data for CRC
  spi_delay();
  spi_tx_byte(0xff);   // dummy data for CRC

#if DEBUG
  break_point();
#endif

  unselect_32u4();

  return 1;

}

uint8_t sd_reader_write_block(uint32_t arg)
{
  // send back to 32u4
  select_32u4();

  spi_tx_byte(0x0);   // signal success
  spi_delay();

  while (spi_rx_byte() != 0xfe);  // wait beginning of transfer
    spi_delay();

  for (uint16_t i = 0 ; i < 512 ; i++) // send block
  {
    buffer[i] = spi_rx_byte();
    spi_delay();
  }

  spi_rx_byte();   // receive dummy CRC
  spi_delay();
  spi_rx_byte();   // receive dummy CRC

  unselect_32u4();

  // write block from card
  if (!card.writeBlock(arg, buffer))
    return 0;
  else
    return 1;

}


