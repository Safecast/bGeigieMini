
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

/* Custom command to request CID and CSD */
#define CMD_REQ_INFO 0x3e

/* indicate to 32u4 to wait and retry */
#define R1_WAIT_RETRY 0xfa
#define R1_SUCCESS 0x00
#define R1_FAILURE 0xff


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
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);
  SPI.setClockDivider(SPI_CLOCK_DIV4);  // 2X speed not working now
  SPI.begin();

  pinMode(sd_detect, INPUT);
  digitalWrite(sd_detect, HIGH);

  pinMode(led, OUTPUT);     // LED
  digitalWrite(led, LOW);  // turn LED on

  pinMode(irq_32u4, INPUT); // IRQ
  BG_SD_READER_INTP();     // set pin change interrupt

  // initialize buffer to zero
  for (int i = 0 ; i < SD_READER_BUF_SIZE ; i++)
    buffer[i] = 0x0;

  // state state to IDLE
  if (sd_reader_init())
  {
    Serial.println("SD card activated.");
    sd_reader_state = SD_READER_ACTIVE;
  } 
  else
  {
    Serial.println("Failed to activate SD card.");
    sd_reader_state = SD_READER_IDLE;
  }
  //sd_reader_state = SD_READER_IDLE;
}

void sd_reader_loop()
{
  unsigned long now = millis();
  if (sd_reader_state == SD_READER_ACTIVE && now - last_interrupt > SD_READER_TIMEOUT)
  {
    Serial.println("SD reader timeout. Turning off.");
    sd_power_off();
    sd_reader_state = SD_READER_IDLE;
  }
}

uint8_t sd_reader_init()
{
  uint8_t status;

  // if the reader is locked, fail
  if (sd_reader_state == SD_READER_DISABLED)
    return 0;

  // if card not inserted, return false
  if (!sd_card_present())
    return 0;

  // first turn off SD card for a little while
  sd_power_off();
  delay(10);

  // power on SD card
  sd_power_on();

  // initialize card (starts SPI as well)
  status = card.init(SPI_HALF_SPEED, cs_sd);

  // change state to active
  if (status)
    sd_reader_state = SD_READER_ACTIVE;
    
  return status;
}

// disable the SD reader
uint8_t sd_reader_lock()
{
  if (sd_reader_state != SD_READER_ACTIVE)
  {
    sd_reader_state = SD_READER_DISABLED;
    return 1;
  }

  // if the SD reader is active we return failure
  return 0;
}

// enable the SD reader
void sd_reader_unlock()
{
  if (sd_reader_state == SD_READER_DISABLED)
    sd_reader_state = SD_READER_IDLE;
}

// wrapper for receiving byte with SPI
uint8_t spi_rx_byte()
{
  return SPI.transfer(0x0);
}

// wrapper for sending byte with SPI
void spi_tx_byte(uint8_t b)
{
  SPI.transfer(b);
}

ISR(BG_32U4_IRQ)
{
  uint8_t cmd = 0;
  uint32_t arg = 0;
  uint8_t ret = 0;

  uint8_t sreg_old = SREG;
  cli(); // disable interrupt

  // we need to check the pin direction
  int pinval = digitalRead(irq_32u4);

  if (pinval)
  {
    // light LED off
    digitalWrite(led, HIGH);

    // update timestamp
    last_interrupt = millis();

    // get command from 32u4
    // This is 5 SPI transfers
    select_32u4();

    // rx command
    cmd = spi_rx_byte();
    spi_delay();
    // rx argument
    arg = spi_rx_byte();
    spi_delay();
    arg = (arg << 8) + spi_rx_byte();
    spi_delay();
    arg = (arg << 8) + spi_rx_byte();
    spi_delay();
    arg = (arg << 8) + spi_rx_byte();
    spi_delay();

    unselect_32u4();

    //Serial.print(cmd, HEX);
    //Serial.print(" ");
    //Serial.println(arg, HEX);

    // if SD card is not enabled, initialize
    if (!sd_reader_init())
    {
      select_32u4();
      spi_tx_byte(R1_WAIT_RETRY); // fail value
      unselect_32u4();
    }

#if DEBUG
    if (cmd == 0x0)
      break_point();
#endif

    if (SD_READER_ACTIVE)
    {
      // action time!
      switch (cmd)
      {
        case 0x40 | CMD_SD_ON:
          // if we get here, it is success!
          select_32u4();
          spi_tx_byte(R1_SUCCESS);
          unselect_32u4();
          break;

        case 0x40 | CMD_SD_OFF:
          // turn SD card off
          sd_power_off();
          sd_reader_state = SD_READER_IDLE;
          // send response
          select_32u4();
          spi_tx_byte(R1_SUCCESS);
          unselect_32u4();
          break;

        case 0x40 | CMD_READ_SINGLE_BLOCK:
          sd_reader_read_block(arg);
          break;

        case 0x40 | CMD_WRITE_SINGLE_BLOCK:
          sd_reader_write_block(arg);
          break;

        case 0x40 | CMD_REQ_INFO:
          sd_reader_get_info();
          break;
        
        default:
          // return fail value
          select_32u4();
          spi_tx_byte(0xff);
          unselect_32u4();
          break;
      }
    }

    // light LED on
    digitalWrite(led, LOW);
  }

  SREG = sreg_old; // enable interrupt

}

uint8_t sd_reader_read_block(uint32_t arg)
{
  // read block from card
  if (!card.readBlock(arg >> 9, buffer))
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

  spi_tx_byte(0x12);   // dummy data for CRC
  spi_delay();
  spi_tx_byte(0x34);   // dummy data for CRC

  unselect_32u4();

  return 1;

}

uint8_t sd_reader_write_block(uint32_t arg)
{
  // send back to 32u4
  select_32u4();

  spi_tx_byte(0x0);   // signal success
  spi_delay();

  spi_delay();
  spi_delay();
  spi_delay();
  spi_rx_byte();      // was for magic 0xEF (not necessary really)
  spi_delay();
  spi_delay();


  for (uint16_t i = 0 ; i < 512 ; i++) // send block
  {
    buffer[i] = spi_rx_byte();
    spi_delay();
  }

  spi_tx_byte(0xAB);   // receive dummy CRC
  spi_delay();
  spi_tx_byte(0xCD);   // receive dummy CRC
  spi_delay();

  unselect_32u4();

  // write block from card
  if (!card.writeBlock(arg >> 9, buffer))
  {
    select_32u4();
    spi_delay();
    //spi_tx_byte(0x0); // failure code
    spi_tx_byte(0xff); // always success
    unselect_32u4();
    return 0;
  }
  else
  {
    select_32u4();
    spi_delay();
    spi_tx_byte(0xff); // success code
    unselect_32u4();
    return 1;
  }

}

void sd_reader_get_info()
{
  int i;

  if (!card.readCID((cid_t *)buffer))
  { // fail
    select_32u4();
    spi_tx_byte(0xff);
    unselect_32u4();
    return;
  }

  if (!card.readCSD((csd_t *)(buffer+16)))
  { // fail
    select_32u4();
    spi_tx_byte(0xff);
    unselect_32u4();
    return;
  }

  select_32u4();

  // success!!
  spi_tx_byte(0x00);

  spi_delay();

  // send CID
  for (i = 0 ; i < 16 ; i++)
  {
    spi_tx_byte(buffer[i]);
    delayMicroseconds(20);  // slightly longer delay to allow some processing in slave
  }

  // CRC
  spi_tx_byte(0x66);
  delayMicroseconds(20);
  spi_tx_byte(0x66);
  delayMicroseconds(20);
  
  // send CSD
  for (i = 0 ; i < 16 ; i++)
  {
    spi_tx_byte(buffer[16+i]);
    delayMicroseconds(20);
  }

  // CRC
  spi_tx_byte(0x56);
  spi_delay();
  spi_tx_byte(0x78);

  unselect_32u4();
  
  return;
}



