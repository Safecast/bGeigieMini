/*
   Simple library to read Geolocation info from a hardware Serial GPS module

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

#include "GPS.h"

// Constructor
GPS::GPS(HardwareSerial serial)
{
  _serial = serial; // Hardware Serial connection, supposed to be initialized
  i = 0;            // character counter initialization
  _updating = 1;    // set to updating until we receive something
}

// Availability indicator
int GPS::available()
{
  // location available when not uploading
  return _updating;
}

// Return the millisecond microprocessor time of when data was received
unsigned long GPS::age()
{
  unsigned long now = millis();
  
  if (now >= _rx_time)
    return now - _rx_time;
  else
    return (ULONG_MAX - _rx_time + now);
}

// Update routine
void GPS::update()
{

  char tmp[25];
  char *tok[SYM_SZ] = {0};
  int j = 0;

  while (_serial.available())
  {
    if (_index < LINE_SZ)
    {
      _line[_index] = _serial.read();
      if (_line[_index] == '\n')
      {
        // set timestamp
        _rx_time = millis();

        // terminate string with null character
        _line[_index+1] = '\0';
        
        // reset character counter
        _index=0;

        // dump the raw GPS data
        //_serial.print(_line);

        // tokenize line
        tok[j] = strtok(_line, ",");
        do
        {
           tok[++j] = strtok(NULL, ",");
        } 
        while ((j < SYM_SZ) && (tok[j] != NULL));

        // reset the index variable for next run
        j = 0;

        // parse line and date/time and update gps struct
        if (strcmp(tok[0], "$GPRMC") == 0)
        {
          parse_line_rmc(tok);
          parse_datetime();
        }
        else if (strcmp(tok[0], "$GPGGA") == 0)
        {
           parse_line_gga(tok);
        }
        
        // clear the flag so that we know its okay to read the data
        _updating = 0;
      }
      else
      {
        // still taking in data. increment index and make sure the updating flag is set
        _updating = 1;
        _index++;
      }
    }
    else
    {
      _index = 0;
    }
  }
  
}

// Compute checksum of input array
static char GPS::checksum(char *s, int N)
{
  int i = 0;
  char chk = s[0];

  for (i=1 ; i < N ; i++)
    chk ^= s[i];

  return chk;
}

// Parse RMC sentence
void GPS::parse_line_rmc(char **token)
{
  memcpy(&_gps_data.utc,        token[1],     UTC_SZ-1);
  memcpy(&_gps_data.status,     token[2],     DEFAULT_SZ-1);
  memcpy(&_gps_data.lat,        token[3],     LAT_SZ-1);
  memcpy(&_gps_data.lat_hem,    token[4],     DEFAULT_SZ-1);
  memcpy(&_gps_data.lon,        token[5],     LON_SZ-1);
  memcpy(&_gps_data.lon_hem,    token[6],     DEFAULT_SZ-1);
  memcpy(&_gps_data.speed,      token[7],     SPD_SZ-1);
  memcpy(&_gps_data.course,     token[8],     CRS_SZ-1);
  memcpy(&_gps_data.date,       token[9],     DATE_SZ-1);
  memcpy(&_gps_data.checksum,   token[10],    CKSUM_SZ-1);
}

// Parse GGA sentence
void GPS::parse_line_gga(char **token)
{
    memcpy(&_gps_data.quality,    token[6], DEFAULT_SZ-1);
    memcpy(&_gps_data.num_sat,   token[7], NUM_SAT_SZ-1);
    memcpy(&_gps_data.precision, token[8], PRECISION_SZ-1);
    memcpy(&_gps_data.altitude,  token[9], ALTITUDE_SZ-1);
}


// Parse date and time from GPS and input in structure
void GPS::parse_datetime()
{
    memset(&_gps_data.datetime, 0, sizeof(date_time_t));

    // parse UTC time
    memcpy(_gps_data.datetime.hour, &_gps_data.utc[0], 2);
    memcpy(_gps_data.datetime.minute, &_gps_data.utc[2], 2);
    memcpy(_gps_data.datetime.second, &_gps_data.utc[4], 2);

    // parse UTC calendar
    memcpy(_gps_data.datetime.day, &_gps_data.date[0], 2);
    memcpy(_gps_data.datetime.month, &_gps_data.date[2], 2);
    memcpy(_gps_data.datetime.year, &_gps_data.date[4], 2);
}

