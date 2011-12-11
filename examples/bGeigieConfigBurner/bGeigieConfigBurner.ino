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
  if (arg_cnt != 2)
  {
    Serial.print("Synthax: setid <id>    id is ");
    Serial.print(BMRDD_ID_LEN);
    Serial.println(" characters long");
    return;
  }

  int len = 0;
  while (args[1][len] != NULL)
    len++;

  if (len != BMRDD_ID_LEN)
  {
    Serial.print("Synthax: setid <id>     id is ");
    Serial.print(BMRDD_ID_LEN);
    Serial.println(" characters long");
    return;
  } 

  for (int i=0 ; i < BMRDD_ID_LEN ; i++)
  {
    EEPROM.write(BMRDD_EEPROM_ID+i, byte(args[1][i]));
    dev_id[i] = args[1][i];
  }
  Serial.print("Device id: ");
  Serial.println(dev_id);
}

void cmdGetId(int arg_cnt, char **args)
{
  pullDevId();
  Serial.print("Device id: ");
  Serial.println(dev_id);
}
