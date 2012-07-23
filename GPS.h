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

// MTK Protocol messages
#define MTK_STANDBY_MODE "$PMTK161,0*28"
#define MTK_STANDBY_MODE_ACK "$PMTK161,0*28"
#define MTK_STANDY_MODE_QUIT_ACK "$PMTK010,002*2D"

#define MTK_PERIODIC_MODE "$PMTK225,2,3000,12000,18000,72000*15"
#define MTK_PERIODIC_MODE_ACK "$PMTK001,225,3*35"

#define MTK_NORMAL_MODE "$PMTK225,0*2B"
#define MTK_NORMAL_MODE_ACK "$PMTK001,225,3*35"

#define MTK_ALWAYSLOCATE_MODE "$PMTK225,8*23"
#define MTK_ALWAYSLOCATE_MODE_ACK "$PMTK001,225,3*35"

#define MTK_BAUDRATE_FACTORY_DEFAULT "$PMTK251,0*28"
#define MTK_BAUDRATE_4800 "$PMTK251,4800*14"
#define MTK_BAUDRATE_9600 "$PMTK251,9600*17"
#define MTK_BAUDRATE_19200 "$PMTK251,19200*22"
#define MTK_BAUDRATE_38400 "$PMTK251,38400*27"
#define MTK_BAUDRATE_57600 "$PMTK251,57600*2C"
#define MTK_BAUDRATE_115200 "$PMTK251,115200*1F"

#define MTK_SET_NMEA_OUTPUT_ALLDATA "$PMTK314,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0*28"
#define MTK_SET_NMEA_OUTPUT_RMCGGA "$PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28"
#define MTK_SET_NMEA_OUTPUT_OFF "$PMTK314,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28"
#define MTK_SET_NMEA_OUTPUT_ACK "$PMTK001,314,3*36"

#define MTK_UPDATE_RATE_1HZ "$PMTK220,1000*1F"
#define MTK_UPDATE_RATE_ACK "$PMTK001,220,3*30"

// time structure
typedef struct
{
    char day[3];
    char month[3];
    char year[3];
    char hour[3];
    char minute[3];
    char second[5];
} date_time_t;

// gps data structure
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

// 'public' methods
void gps_init(HardwareSerial *serial, char *line);
void gps_update();
int gps_available();
gps_t *gps_getData();
char gps_checksum(char *s, int N);
unsigned long gps_age();

#endif /* GPS_H */
