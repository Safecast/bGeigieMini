/*
   Simple library to read Geolocation info from a Serial GPS module
   Copyright (C) 2011 Robin Scheibler aka FakuFaku, Christopher Wang aka Akiba

   Permission is hereby granted, free of charge, to any person obtaining a copy of
   this software and associated documentation files (the "Software"), to deal in
   the Software without restriction, including without limitation the rights to
   use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
   of the Software, and to permit persons to whom the Software is furnished to do
   so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/

#ifndef GPS_H
#define GPS_H

// Link to arduino library
#if ARDUINO >= 100
#include <Arduino.h>
#else
#include <WProgram.h>
#endif

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
    inline gps_t *getData() { return &_gps_data; }
    static char checksum();
    unsigned long age();

  // private
  private:
    byte _updating;
    HardwareSerial _serial;       // The serial port used by GPS
    gps_t _gps_data;              // GPS data structure
    char _line[LINE_SZ];          // buffer to receive new line from serial
    byte _index;                  // current character index
    unsigned long _rx_time;       // Timestamp of received time
    void parse_line_rmc(char **token);  // parse RMC sentence (from NMEA protocol)
    void parse_line_gga(char **token);  // parse GGA sentence (from NMEA protocol)
    void parse_datetime();              // parse date and time into correct data struct
}

#endif /* GPS_H */
