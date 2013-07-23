#ifndef __CONFIG_H__
#define __CONFIG_H__

/****** ************* ******/
/* User changeable options */
/****** ************* ******/

/****** **************************** ******/
/* Do not changes things lightly          */
/* These could disable important features */
/****** **************************** ******/

#include <stdint.h>
#include <stdlib.h>

#include <SD.h>

// Enable or Disable features
#define RADIO_ENABLE 1
#define SD_READER_ENABLE 1
#define BG_PWR_ENABLE 1
#define CMD_LINE_ENABLE 1

/* Battery options */
#define BATT_LOW_VOLTAGE 3700       // indicate battery low when this voltage is reached
#define BATT_SHUTDOWN_VOLTAGE 3300  // lowest voltage acceptable by power regulator

/* Radio options */
#define DEST_ADDR 0xFFFF      // this is the 802.15.4 broadcast address
#define RX_ADDR_BASE 0x3000   // base for radio address. Add device number to make complete address
#define CHANNEL 20            // transmission channel

/* the configuration of the device */
#define CONFIG_EEPROM_ADDR 100

/* id is a 3-digit hex number starting with 3 */
#define IS_BOOLEAN(x) (((~1)&x) == 0)

#define CONFIG_ID_INVALID 0xFFFF
#define CONFIG_SO_INVALID 0xFF
#define CONFIG_CT_INVALID 0xFF
#define CONFIG_HV_INVALID 0xFF
#define CONFIG_SD_INVALID 0xFF

#define CONFIG_SO_DEFAULT 1
#define CONFIG_CT_DEFAULT 0
#define CONFIG_HV_DEFAULT 0
#define CONFIG_SD_DEFAULT 0

#define CONFIG_MAGIC 0xBEEF

/* config file name */
#define CONFIG_FILE_NAME "BGCONFIG.TXT"
#define CONFIG_MAX_KEY_SZ 32
#define CONFIG_MAX_VAL_SZ 32
#define CONFIG_KEYVAL_SEP ":"

/* A structure containing the configuration */
typedef struct
{
  /* magic : allows to check if EEPROM has been written or not */
  uint16_t magic;
  /* device id : default is 0xFFFFFFFF i.e. id not defined */
  uint16_t id;
  /* Serial output enabled (1) / disabled (0) */
  uint8_t serial_output;
  /* JP coordinate truncation Enable (1) / Disable (0) */
  uint8_t coord_truncation;
  /* HV board sensing */
  uint8_t hv_sense;
  /* SD reader is read/write Enable (1) / Disable (0) */
  uint8_t sd_rw;
} config_t;

/* the configuration */
extern config_t theConfig;

extern char PROGMEM ID_K[];
extern char PROGMEM SO_K[];
extern char PROGMEM CT_K[];
extern char PROGMEM HV_K[];
extern char PROGMEM SD_K[];

/* all the function definitions */
void config_init();
void cfgSetDefault(config_t *cfg);
void cfgSetInvalid(config_t *cfg);
void cfgcpy(config_t *dst, config_t *src);
int configFromFile(config_t *cfg);
int configToFile();
void configFromEEPROM(config_t *cfg);
void configToEEPROM();
void readEEPROM(int addr, uint8_t *ptr, size_t len);
void writeEEPROM(int addr, uint8_t *ptr, size_t len);

/* helper functions */
int readNextLine(File *file, char *buf, int max_len);
int writeKeyVal(File *file, char *key, char *val);

#endif /* __CONFIG_H__ */
