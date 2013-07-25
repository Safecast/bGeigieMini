
#include <EEPROM.h>

#include "config.h"

/* our configuration global variable */
config_t theConfig = {CONFIG_ID_INVALID, 1, 0, 0, 0};
int config_file_exists  = 0;
int config_file_rewrite = 0;

/* config key values */
char PROGMEM ID_K[] = "ID";
char PROGMEM SO_K[] = "SerialOutput";
char PROGMEM CT_K[] = "CoordTrunc";
char PROGMEM HV_K[] = "HVSense";
char PROGMEM SD_K[] = "SDRW";

/* config filename */
char config_filename[] = CONFIG_FILE_NAME;

/* this flag decides is the config file needs to be rewritten from flash */

/* initialize the configuration in setup */
void config_init()
{
  config_t fromFile;

  int config_file_exists = 0;
  int rewrite_eeprom_flag = 0;

  /* we rely on SD card being opened by the SD logger */
  /* otherwise, we'd need to call SD.begin(pin) */

  // First, pull whatever configuration is in the EEPROM
  configFromEEPROM(&theConfig);

  // set default values if magic is not right
  if (theConfig.magic != CONFIG_MAGIC)
  {
    cfgSetInvalid(&theConfig); // first set all to invalid
    rewrite_eeprom_flag = 1;
  }

  // now try pulling from the file, if it exists
  config_file_exists = SD.exists(config_filename);
  if (config_file_exists)
  {
    /* read configuration file */
    configFromFile(&fromFile);

    /* compare all values from EEPROM and file, if value in file is valid, replace in EEPROM */
    if (fromFile.id != theConfig.id && fromFile.id != CONFIG_ID_INVALID)
    {
      theConfig.id = fromFile.id;
      rewrite_eeprom_flag = 1;
    }
    if (fromFile.serial_output != theConfig.serial_output && IS_BOOLEAN(fromFile.serial_output))
    {
      theConfig.serial_output = fromFile.serial_output;
      rewrite_eeprom_flag = 1;
    }
    if (fromFile.coord_truncation != theConfig.coord_truncation && IS_BOOLEAN(fromFile.coord_truncation))
    {
      theConfig.coord_truncation = fromFile.coord_truncation;
      rewrite_eeprom_flag = 1;
    }
    if (fromFile.hv_sense != theConfig.hv_sense && fromFile.hv_sense != CONFIG_ID_INVALID)
    {
      theConfig.hv_sense = fromFile.hv_sense;
      rewrite_eeprom_flag = 1;
    }
    if (fromFile.sd_rw != theConfig.sd_rw && fromFile.sd_rw != CONFIG_ID_INVALID)
    {
      theConfig.sd_rw = fromFile.sd_rw;
      rewrite_eeprom_flag = 1;
    }
  }

  /* check all the values in EEPROM and set to default if necessary */
  cfgSetDefault(&theConfig);

  // write to EEPROM if necessary
  if (rewrite_eeprom_flag)
    configToEEPROM();

  // write the file if necessary
  if (!config_file_exists)
    configToFile();
}

/* set default values if invalid */
void cfgSetDefault(config_t *cfg)
{
  /* there is no a priori info on id value */
  /* for all other values, we can only check if boolean or not */
  if (!IS_BOOLEAN(cfg->serial_output))
    cfg->serial_output    = CONFIG_SO_DEFAULT;
  if (!IS_BOOLEAN(cfg->serial_output))
    cfg->coord_truncation = CONFIG_CT_DEFAULT;
  if (!IS_BOOLEAN(cfg->serial_output))
    cfg->hv_sense         = CONFIG_HV_DEFAULT;
  if (!IS_BOOLEAN(cfg->serial_output))
    cfg->sd_rw            = CONFIG_SD_DEFAULT;
}

/* initialize structure to all invalid */
void cfgSetInvalid(config_t *cfg)
{
  cfg->magic            = CONFIG_MAGIC;
  cfg->id               = CONFIG_ID_INVALID;
  cfg->serial_output    = CONFIG_SO_INVALID;
  cfg->coord_truncation = CONFIG_CT_INVALID;
  cfg->hv_sense         = CONFIG_HV_INVALID;
  cfg->sd_rw            = CONFIG_SD_INVALID;
}

/* copy src into dst */
void cfgcpy(config_t *dst, config_t *src)
{
  memcpy((void *)dst, (void *)src, sizeof(config_t));
}

/* read configuration from SD card */
int configFromFile(config_t *cfg)
{
  File file;
  char fline[CONFIG_MAX_KEY_SZ+CONFIG_MAX_VAL_SZ+1];
  char *key, *val;
  int n_param_found = 0;

  /* initialize the config var to all invalid */
  cfgSetInvalid(cfg);

  /* open file */
  file = SD.open(config_filename, FILE_READ);

  /* return 0 if file not found */
  if (file)
  {

    /* read the first line */
    while ((readNextLine(&file, fline, CONFIG_MAX_KEY_SZ+CONFIG_MAX_VAL_SZ+2)) != -1)
    {
      // split in key/val using specified delimiter
      key = val = fline;
      key = strsep(&val, CONFIG_KEYVAL_SEP);

      /* parse value */
      char *endptr;
      uint32_t v = (uint32_t)strtoul(val, &endptr, 16);

      /* if parse is not successful, skip line */
      if (*endptr != '\0')
        continue;

      // check against possible options
      if (strcmp_P(key, ID_K) == 0)
        cfg->id = (uint16_t)v;

      else if (strcmp_P(key, SO_K) == 0)
        cfg->serial_output = (uint8_t)v;

      else if (strcmp_P(key, CT_K) == 0)
        cfg->coord_truncation = (uint8_t)v;

      else if (strcmp_P(key, HV_K) == 0)
        cfg->hv_sense = (uint8_t)v;

      else if (strcmp_P(key, SD_K) == 0)
        cfg->sd_rw = (uint8_t)v;

      else
        continue;

      // increase number of parameters found
      n_param_found++;
    }
  }

  // close the file
  file.close();

  return n_param_found;
}


/* write the current configuration to a file */
/* the existing file is deleted first */
int configToFile()
{
  File cfile;
  char key[CONFIG_MAX_KEY_SZ];
  char val[CONFIG_MAX_VAL_SZ];

  // Remove the file if already there
  SD.remove(config_filename);

  /* create new file */
  cfile = SD.open(config_filename, FILE_WRITE);
  if (!cfile)
    return 0;

  /* write ID */
  strcpy_P(key, ID_K);
  sprintf(val, "%lx", (unsigned long)theConfig.id);
  writeKeyVal(&cfile, key, val);

  /* write serial output option */
  strcpy_P(key, SO_K);
  sprintf(val, "%u", (unsigned int)theConfig.serial_output);
  writeKeyVal(&cfile, key, val);
  
  /* write coordinate truncation option */
  strcpy_P(key, CT_K);
  sprintf(val, "%u", (unsigned int)theConfig.coord_truncation);
  writeKeyVal(&cfile, key, val);

  /* write HV sense option */
  strcpy_P(key, HV_K);
  sprintf(val, "%u", (unsigned int)theConfig.hv_sense);
  writeKeyVal(&cfile, key, val);

  /* write SD-RW option */
  strcpy_P(key, SD_K);
  sprintf(val, "%u", (unsigned int)theConfig.sd_rw);
  writeKeyVal(&cfile, key, val);

  /* close file */
  cfile.close();

  /* success */
  return 1;
}

/* read config from EEPROM into cfg variable */
void configFromEEPROM(config_t *cfg)
{
  readEEPROM(CONFIG_EEPROM_ADDR, (uint8_t *)cfg, sizeof(config_t));
}

/* write theConfig to EEPROM */
void configToEEPROM()
{
  writeEEPROM(CONFIG_EEPROM_ADDR, (uint8_t *)&theConfig, sizeof(config_t));
}

/* read a number of byte from EEPROM */
void readEEPROM(int addr, uint8_t *ptr, size_t len)
{
  cli(); // disable all interrupts

  for (unsigned int i=0 ; i < len ; i++)
    ptr[i] = (uint8_t)EEPROM.read(i+addr);

  sei(); // re-enable all interrupts
}

/* write to EEPROM a number of bytes */
void writeEEPROM(int addr, uint8_t *ptr, size_t len)
{
  cli(); // disable all interrupts

  for (unsigned int i=0 ; i < len ; i++)
    EEPROM.write(i+addr, ptr[i]);

  sei(); // re-enable all interrupts
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

/* write a key/val pair to a file */
void writeKeyVal(File *file, char *key, char *val)
{
  file->write(key);
  file->write(CONFIG_KEYVAL_SEP);
  file->write(val);
  file->write('\n');
}

