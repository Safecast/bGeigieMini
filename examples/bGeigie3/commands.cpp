
#include "commands.h"

#include <stdlib.h>

/**************************/
/* command line functions */
/**************************/

void registerCommands()
{
  cmdAdd("help", cmdPrintHelp);
  cmdAdd("setid", cmdSetId);
  cmdAdd("getid", cmdGetId);
  cmdAdd("verbose", cmdVerbose);
}

void printHelp()
{
  char tmp[50];
  strcpy_P(tmp, PSTR("Device id: "));
  Serial.println(tmp);
  Serial.println(dev_id);
  strcpy_P(tmp, PSTR("List of commands:"));
  Serial.println(tmp);
  strcpy_P(tmp, PSTR("  Set new device id:     setid <id>    id is "));
  Serial.print(tmp);
  Serial.print(BMRDD_ID_LEN);
  strcpy_P(tmp, PSTR(" characters long"));
  Serial.println(tmp);
  strcpy_P(tmp, PSTR("  Get device id:         getid"));
  Serial.println(tmp);
  strcpy_P(tmp, PSTR("  Enable log to serial:  verbose on"));
  Serial.println(tmp);
  strcpy_P(tmp, PSTR("  Disable log to serial: verbose off"));
  Serial.println(tmp);
  strcpy_P(tmp, PSTR("  Show this help:        help"));
  Serial.println(tmp);
}

void cmdPrintHelp(int arg_cnt, char **args)
{
  printHelp();
}  

void cmdVerbose(int arg_cnt, char **args)
{
  if (arg_cnt != 2)
    goto error;

  if (strncmp_P(args[1], PSTR("on"), 2) == 0)
  {
    serial_output_enable = 1;
    return;
  }
  else if (strncmp_P(args[1], PSTR("off"), 3) == 0)
  {
    serial_output_enable = 0;
    return;
  }

error:
  char tmp[30];
  strcpy_P(tmp, "Synthax: verbose <on/off>");
  Serial.println(tmp);
  return;
}

/* set new device id in EEPROM */
void cmdSetId(int arg_cnt, char **args)
{
  char tmp[40];
  int len = 0;

  if (arg_cnt != 2)
    goto errorSetId;

  while (args[1][len] != NULL)
    len++;

  if (len != BMRDD_ID_LEN)
    goto errorSetId;

  // write ID to EEPROM
  for (int i=0 ; i < BMRDD_ID_LEN ; i++)
    EEPROM.write(BMRDD_EEPROM_ID+i, byte(args[1][i]));

  // pull dev id from the EEPROM so that we check it was successfully written
  pullDevId();
  strcpy_P(tmp, PSTR("Device id: "));
  Serial.print(tmp);
  Serial.print(dev_id);
  if (strncmp(dev_id, args[1], BMRDD_ID_LEN) == 0)
    strcpy_P(tmp, PSTR(" - success."));
  else
    strcpy_P(tmp, PSTR(" - failure."));
  Serial.println(tmp);
    
  return; // happyily

errorSetId:
  strcpy_P(tmp, PSTR("Synthax: setid <id>     id is "));
  Serial.print(tmp);
  Serial.print(BMRDD_ID_LEN);
  strcpy_P(tmp, PSTR(" characters long"));
  Serial.println(tmp);
}

void cmdGetId(int arg_cnt, char **args)
{
  pullDevId();
  Serial.print("Device id: ");
  Serial.println(dev_id);
}

