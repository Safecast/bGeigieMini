#ifndef GPS_H
#define GPS_H

#define LINE_SZ         100
#define SYM_SZ          20
#define GPS_TYPE_SZ     10
#define UTC_SZ          10
#define LAT_SZ          15
#define LON_SZ          15
#define SPD_SZ          8
#define CRS_SZ          8
#define DATE_SZ         7
#define CKSUM_SZ        6
#define RMC_FLD_SZ      11
#define NUM_SAT_SZ      3
#define PRECISION_SZ    5
#define ALTITUDE_SZ     8
#define DEV_NAME_SZ     30
#define MEAS_TYPE_SZ    20
#define DEFAULT_SZ      2

typedef struct
{
    char day[3];
    char month[3];
    char year[3];
    char hour[3];
    char minute[3];
    char second[5];
} date_time_t;

typedef struct
{
    byte updating;
    char gps_type[GPS_TYPE_SZ];
    char utc[UTC_SZ];
    char quality[DEFAULT_SZ];
    char status[DEFAULT_SZ];
    char lat[LAT_SZ];
    char lat_hem[DEFAULT_SZ];
    char lon[LON_SZ];
    char lon_hem[DEFAULT_SZ];
    char speed[SPD_SZ];
    char course[CRS_SZ];
    char date[DATE_SZ+1];
    char checksum[CKSUM_SZ];
    char num_sat[NUM_SAT_SZ];
    char precision[PRECISION_SZ];
    char altitude[ALTITUDE_SZ];
    char dev_name[DEV_NAME_SZ];
    char meas_type[MEAS_TYPE_SZ];
    date_time_t datetime;
} gps_t;


class GPS
{
  // public
  public:
    void GPS(HardwareSerial serial);
    void udpate();
    int available();

  // private
  private:
    HardwareSerial _serial; // The serial port used by GPS
    gps_t _gps_data;        // GPS data structure
    char line[LINE_SZ];     // buffer to receive new line from serial
    byte i;                 // current character index
}

#endif /* GPS_H */
