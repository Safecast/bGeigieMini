#ifndef __CONFIG_H__
#define __CONFIG_H__

/****** ************* ******/
/* User changeable options */
/****** ************* ******/

/****** **************************** ******/
/* Do not changes things lightly          */
/* These could disable important features */
/****** **************************** ******/

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
#define DEV_ID_VALID(x) ((x >> 8) == 0x3)
#define DEV_ID_INVALID (~(uint32_t)0)
#define IS_BOOLEAN(x) (((~1)&x) == 0)

/* config file name */
#define CONFIG_FILE_NAME "bgeigie.cfg"
#define CONFIG_MAX_KEY_SZ 32
#define CONFIG_MAX_VAL_SZ 32
#define CONFIG_KEYVAL_SEP ":"

/* the configuration */
extern config_t theConfig;

/* all the function definitions */
void pushToEEPROM(int addr, uint8_t *ptr, size_t len);
void pullFromEEPROM(int addr, uint8_t *ptr, size_t len);

#endif /* __CONFIG_H__ */
