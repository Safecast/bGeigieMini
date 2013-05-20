
#include "config.h"

#include <stdlib.h>

/* A structure containing the configuration */
typedef struct
{
  /* device id : default is 0xFFFFFFFF i.e. id not defined */
  uint32_t id = -1;
  /* Serial output enabled (1) / disabled (0) */
  uint8_t serial_output = 0;
  /* JP coordinate truncation Enable (1) / Disable (0) */
  uint8_t coord_truncation = 0;
  /* HV board sensing */
  uint8_t hv_sense = 0;
  /* SD reader is read/write Enable (1) / Disable (0) */
  uint8_t sd_rw = 0;
} config_t;

/* our configuration global variable */
config_t theConfig;

/* this flag decides is the config file needs to be rewritten from flash */
config_rewrite_flag = 0;

/* initialize the configuration in setup */
void config_init()
{
}

void config_update()
{

}

int pullFromFile()
{
  File file;
  char fline[CONFIG_MAX_KEY_SZ+CONFIG_MAX_VAL_SZ+1];
  int nc = CONFIG_MAX_KEY_SZ + CONFIG_MAX_VAL_SZ;
  char *key, *val;
  int ksz, vsz;
  int r;
  int rewrite_eeprom_flag = 0;

  // First, pull whatever configuration is in the EEPROM
  pullFromEEPROM(CONFIG_EEPROM_ADDR, &theConfig, sizeof(config_t));

  /* we rely on SD card being opened by the SD logger */
  /* otherwise, we'd need to call SD.begin(pin) */

  /* open file */
  file = SD.open(CONFIG_FILE_NAME, FILE_READ);

  /* return 0 if file not found */
  if (file)
  {

    /* read the first line */
    while ((r = readNextLine(file, fline, nc)) != -1)
    {
      // split in key/val using specified delimiter
      key = fline;
      key = strsep(&val, CONFIG_KEYVAL_SEP);

      /* parse value */
      char **endptr;
      uint32_t v = (uint32_t)strtoul(val, endptr, 16);

      /* if parse is not successful, skip line */
      if (!(*val != NULL && **endptr == NULL))
        continue;

      // check against possible options
      if (strncmp_P(PSTR("ID"), key, 2) == 0)
      {
        if (DEV_ID_VALID(v))
          theConfig.id = v;
        else
          theConfig.id = DEV_ID_INVALID;
      }
      else if (strncmp_P(PSTR("SerialOutput"), key, 12) == 0)
      {
        if (IS_BOOLEAN(v) && theConfig.serial_output != v)
        {
          theConfig.serial_output = v;
          rewrite_eeprom_flag = 1;
        }
        else if (!IS_BOOLEAN(theConfig.serial_output))
          theConfig.serial_output = 0;  // default
      }
      else if (strncmp_P(PSTR("CoordTrunc"), key, 10) == 0)
      {
        if (IS_BOOLEAN(v) && theConfig.coord_truncation != v)
        {
          theConfig.coord_truncation = v;
          rewrite_eeprom_flag = 1;
        }
        else if (!IS_BOOLEAN(theConfig.coord_truncation))
          theConfig.coord_truncation = 0;  // default
      }
      else if (strncmp_P(PSTR("SDRW"), key, 4) == 0)
      {
        if (IS_BOOLEAN(v) && theConfig.sd_rw != v)
        {
          theConfig.sd_rw = v;
          rewrite_eeprom_flag = 1;
        }
        else if (!IS_BOOLEAN(theConfig.sd_rw))
          theConfig.sd_rw = 0;  // default
      }
      else if (strncmp_P(PSTR("HVSense"), key, 7) == 0)
      {
        if (IS_BOOLEAN(v) && theConfig.hv_sense != v)
        {
          theConfig.hv_sense = v;
          rewrite_eeprom_flag = 1;
        }
        else if (!IS_BOOLEAN(theConfig.hv_sense))
          theConfig.hv_sense = 0;  // default
      }
      else
      {
        continue;
      }
    }
  }
  else
  {
    // if the file was not found, mark to be rewritten
    config_rewrite_flag = 1;
  }

  // write to EEPROM if necessary
  if (rewrite_eeprom_flag)
    pushToEEPROM(CONFIG_EEPROM_ADDR, &theConfig, sizeof(config_t));
}

/* this reads at most max_len char from the next line in the file */
/* the buffer needs to be large enough to store max_len+1 char */
int readNextLine(File *file, char *buf, int max_len)
{
  char c = file->read();
  int i = 0;

  if (c == -1)
    return -1;

  /* read all characters until \n or EOF */
  while (c != -1 && i < max_len)
  {
    /* check for EOL */
    if (c == '\n')
      break;
    else
      buf[i] = c;

    /* read next */
    c = file->read();
    i++;
  }

  /* terminate string and return length */
  buf[i] = NULL;
  return i;
}

void PushToFile()
{

}

/* read device id from EEPROM */
void pullFromEEPROM(int addr, uint8_t *ptr, size_t len)
{
  cli(); // disable all interrupts

  for (int i=0 ; i < len ; i++)
    EEPROM.read(i+addr, ptr[i]);

  sei(); // re-enable all interrupts
}

/* write to EEPROM an unsigned integer */
void pushToEEPROM(int addr, uint8_t *ptr, size_t len)
{
  cli(); // disable all interrupts

  for (int i=0 ; i < len ; i++)
    ptr[i] = (uint8_t)EEPROM.write(i+addr);

  // set the end of string null
  dev_id[BMRDD_ID_LEN] = NULL;

  sei(); // re-enable all interrupts
}
