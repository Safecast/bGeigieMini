#ifndef __BGEIGIE_NINJA2__
#define __BGEIGIE_NINJA2__

#include <stdint.h>

#define MAX_DEV_NUM 4
#define DEV_TIMEOUT 15000

// Radioactivity levels and alarms
#define LND7313_CONVERSION_FACTOR 334
#define RAD_ALARM1 0.5
#define RAD_ALARM2 1.0
#define RAD_ALARM3 3.0
#define RAD_ALARM4 5.0

// some data invalidity error constants
#define TEMP_INVALID 1000

typedef struct 
{
  // Link status and age
  unsigned long timestamp;
  uint8_t active;

  // device id
  uint16_t id;

  // status variables
  unsigned long CPM;
  unsigned long total;
  unsigned long uSh_int;
  uint16_t uSh_dec;
  char rad_flag;
  char gps_flag;
  int num_sat;

  // diagnostic variables
  int8_t temperature;
  int8_t humidity;
  int16_t battery;
  int16_t vcc;
  int16_t hv;
  uint8_t sd_status;

  // the date and time
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;

} BGeigieDev;

extern uint8_t dev_index;

#endif /* __BGEIGIE_NINJA2__ */
