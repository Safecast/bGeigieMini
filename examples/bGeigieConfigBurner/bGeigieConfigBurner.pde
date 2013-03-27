/*
 * A simple command interface to program the bGeigie
 */

#include <EEPROM.h>
#include <Cmd.h>

#define BMRDD_EEPROM_ID 100
#define BMRDD_ID_LEN 3

char dev_id[BMRDD_ID_LEN+1];

void setup()
{
  cmdInit(57600);

  pullDevId();

  printHelp();

  cmdAdd("help", cmdPrintHelp);
  cmdAdd("setid", cmdSetId);
  cmdAdd("getid", cmdGetId);
}

void loop()
{
  cmdPoll();
}

void printHelp()
{
  Serial.print("Device id: ");
  Serial.println(dev_id);
  Serial.println("List of commands:");
  Serial.print("  Set new device id: setid <id>    id is ");
  Serial.print(BMRDD_ID_LEN);
  Serial.println(" characters long");
  Serial.println("  Get device id:     getid");
  Serial.println("  Show this help:    help");
}
  

void cmdPrintHelp(int arg_cnt, char **args)
{
  printHelp();
}  

void pullDevId()
{
  for (int i=0 ; i < BMRDD_ID_LEN ; i++)
  {
    dev_id[i] = (char)EEPROM.read(i+BMRDD_EEPROM_ID);
    if (dev_id[i] < '0' || dev_id[i] > '9')
    {
      dev_id[i] = '0';
      EEPROM.write(i+BMRDD_EEPROM_ID, dev_id[i]);
    }
  }
  dev_id[BMRDD_ID_LEN] = NULL;
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
