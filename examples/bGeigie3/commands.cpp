
#include "commands.h"
#include "config.h"

#include <GPS.h>
#include <Cmd.h>
#include <stdlib.h>

/* a couple of strings in flash mem */
char PROGMEM on_str[] = "on";
char PROGMEM off_str[] = "off";
char PROGMEM eeprom_str[] = "eeprom";
char PROGMEM file_str[] = "file";

/* command names */
char help_str[] = "help";
char config_str[] = "config";
char diagnostics_str[] = "diagnostics";
char gpsfullcold_str[] = "gpsfullcold";

/* definitions */
void cmdConfig(int arg_cnt, char **args);
void cmdPrintHelp(int arg_cnt, char **args);
void cmdDiagnostics(int arg_cnt, char **args);
void cmdGPSFullCold(int arg_cnt, char **args);
void showConfig(config_t *cfg);

extern void diagnostics();

/**************************/
/* command line functions */
/**************************/

void registerCommands()
{
  cmdAdd(help_str, cmdPrintHelp);
  cmdAdd(config_str, cmdConfig);
  cmdAdd(diagnostics_str, cmdDiagnostics);
  cmdAdd(gpsfullcold_str, cmdGPSFullCold);
}

void cmdConfig(int arg_cnt, char **args)
{
  char tmp[200];

  if (arg_cnt < 2 || arg_cnt > 3)
    goto help;

  if (strcmp_P(args[1], PSTR("help")) == 0)
  {
    goto help;
  }
  else if (strcmp_P(args[1], PSTR("show")) == 0)
  {
    if (arg_cnt == 2)
      showConfig(&theConfig);
    else if (arg_cnt == 3)
    {
      config_t cfg;
      if (strcmp_P(args[2], eeprom_str) == 0)
      {
        configFromEEPROM(&cfg);
        strcpy_P(tmp, PSTR("EEPROM content:"));
        Serial.print(tmp);
        if (cfg.magic == CONFIG_MAGIC)
          strcpy_P(tmp, PSTR(" with magic"));
        else
          strcpy_P(tmp, PSTR(" without magic"));
        Serial.println(tmp);
      }
      else if (strcmp_P(args[2], file_str) == 0)
      {
        configFromFile(&cfg);
        Serial.println("File content:");
      }
      else
        goto help;
      showConfig(&cfg);
    }
    else
      goto help;
    return;
  }
  else if (strcmp_P(args[1], PSTR("save")) == 0)
  {
    if (arg_cnt == 2)
    {
      configToFile();
      configToEEPROM();
    }
    else if (arg_cnt == 3)
    {
      if (strcmp_P(args[2], eeprom_str) == 0)
        configToEEPROM();
      else if (strcmp_P(args[2], file_str) == 0)
        configToFile();
      else
        goto help;
    }
    Serial.println("Saved.");
    return;
  }

  /* only commands with 3 arguments are left */
  if (arg_cnt != 3)
    goto help;

  if (strcmp_P(args[1], PSTR("copy")) == 0)
  {
    if (strcmp_P(args[2], eeprom_str) == 0)
      configFromEEPROM(&theConfig);
    else if (strcmp_P(args[2], file_str) == 0)
      configFromFile(&theConfig);
    else
      goto help;
    Serial.println("Copied.");

    return;
  }
  else if (strcmp_P(args[1], ID_K) == 0)
  {
    char *endptr = args[2];
    uint32_t tid = (uint32_t)strtol(args[2], &endptr, 16);

    if (*endptr != '\0')
      goto help;
    else
      theConfig.id = tid;

    return;
  }
  else if (strcmp_P(args[1], SO_K) == 0)
  {
    if (strcmp_P(args[2], on_str) == 0)
      theConfig.serial_output = 1;
    else if (strcmp_P(args[2], off_str) == 0)
      theConfig.serial_output = 0;
    else
      goto help;
    return;
  }
  else if (strcmp_P(args[1], CT_K) == 0)
  {
    if (strcmp_P(args[2], on_str) == 0)
      theConfig.coord_truncation = 1;
    else if (strcmp_P(args[2], off_str) == 0)
      theConfig.coord_truncation = 0;
    else
      goto help;
    return;
  }
  else if (strcmp_P(args[1], HV_K) == 0)
  {
    if (strcmp_P(args[2], on_str) == 0)
      theConfig.hv_sense = 1;
    else if (strcmp_P(args[2], off_str) == 0)
      theConfig.hv_sense = 0;
    else
      goto help;
    return;
  }
  else if (strcmp_P(args[1], SD_K) == 0)
  {
    if (strcmp_P(args[2], on_str) == 0)
      theConfig.sd_rw = 1;
    else if (strcmp_P(args[2], off_str) == 0)
      theConfig.sd_rw = 0;
    else
      goto help;
    return;
  }

help:
  strcpy_P(tmp, PSTR("Usage: config <cmd> [args]"));
  Serial.println(tmp);
  strcpy_P(tmp, PSTR("List of commands:"));
  Serial.println(tmp);
  strcpy_P(tmp, PSTR("  config show                    Show current configuration."));
  Serial.println(tmp);
  strcpy_P(tmp, PSTR("  config show [eeprom/file]      Show eeprom/file configuration."));
  Serial.println(tmp);
  strcpy_P(tmp, PSTR("  config ID [id]                 Set device serial number. This is a 4-digit hex number"));
  Serial.println(tmp);
  strcpy_P(tmp, PSTR("  config SerialOutput [on/off]   Toggle serial output on or off."));
  Serial.println(tmp);
  strcpy_P(tmp, PSTR("  config CoordTrunc [on/off]     Enable or disable coordinate truncation to 100x100m grid."));
  Serial.println(tmp);
  strcpy_P(tmp, PSTR("  config HVSense [on/off]        Enable or disable high-voltage output sensing."));
  Serial.println(tmp);
  strcpy_P(tmp, PSTR("  config SDRW [on/off]           Enable or disable write permission to SD card through reader."));
  Serial.println(tmp);
  strcpy_P(tmp, PSTR("  config save                    Writes configuration to EEPROM and SD card."));
  Serial.println(tmp);
  strcpy_P(tmp, PSTR("  config save [eeprom/file]      Writes configuration to EEPROM or SD card."));
  Serial.println(tmp);
  strcpy_P(tmp, PSTR("  config copy [eeprom/file]      Copy configuration from EEPROM or SD card to memory."));
  Serial.println(tmp);
  strcpy_P(tmp, PSTR("  config help                    Show this help."));
  Serial.println(tmp);
  return;
}

void showConfig(config_t *cfg)
{
  char tmp[50];

  strcpy_P(tmp, ID_K);
  Serial.print(tmp);
  Serial.print(':');
  Serial.println((unsigned long)cfg->id, HEX);
  
  strcpy_P(tmp, SO_K);
  Serial.print(tmp);
  Serial.print(':');
  Serial.println(cfg->serial_output);
  
  strcpy_P(tmp, CT_K);
  Serial.print(tmp);
  Serial.print(':');
  Serial.println(cfg->coord_truncation);

  strcpy_P(tmp, HV_K);
  Serial.print(tmp);
  Serial.print(':');
  Serial.println(cfg->hv_sense);

  strcpy_P(tmp, SD_K);
  Serial.print(tmp);
  Serial.print(':');
  Serial.println(cfg->sd_rw);
}

void cmdPrintHelp(int arg_cnt, char **args)
{
  char tmp[100];
  strcpy_P(tmp, PSTR("List of commands:"));
  Serial.println(tmp);
  strcpy_P(tmp, PSTR("  config <cmd> [arg]  Configure device."));
  Serial.println(tmp);
  strcpy_P(tmp, PSTR("  diagnostics         Run diagnostic of device."));
  Serial.println(tmp);
  strcpy_P(tmp, PSTR("  gpsfullcold         Do a full cold restart of the GPS."));
  Serial.println(tmp);
  strcpy_P(tmp, PSTR("  help                Show this help"));
  Serial.println(tmp);
}  

void cmdDiagnostics(int arg_cnt, char **args)
{
  diagnostics();
  return;
}

void cmdGPSFullCold(int arg_cnt, char **args)
{
  char tmp[100];
  strcpy_P(tmp, PSTR(MTK_FULL_COLD_START));
  gps_send_command(tmp);
  strcpy_P(tmp, PSTR("  Issued full cold restart command to the GPS."));
  Serial.println(tmp);
  return;
}
