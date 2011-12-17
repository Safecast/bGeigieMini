/*
   Simple library to read Geolocation info from a Serial GPS module

   Copyright (c) 2011, Robin Scheibler aka FakuFaku, Christopher Wang aka Akiba
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the <organization> nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
    GPS(HardwareSerial *serial, char *line);
    void update();
    int available();
    inline gps_t *getData() { return &_gps_data; }
    static char checksum(char *s, int N);
    unsigned long age();

  // private
  private:
    byte _updating;
    HardwareSerial* _serial;      // The serial port used by GPS
    gps_t _gps_data;              // GPS data structure
    char *_line;                  // buffer to receive new line from serial
    byte _index;                  // current character index
    unsigned long _rx_time;       // Timestamp of received time
    void parse_line_rmc(char **token);  // parse RMC sentence (from NMEA protocol)
    void parse_line_gga(char **token);  // parse GGA sentence (from NMEA protocol)
    void parse_datetime();              // parse date and time into correct data struct
};

#endif /* GPS_H */
