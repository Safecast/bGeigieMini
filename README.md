# Safecast bGeigie Library

SafecastBGeigie is the code for the firmware needed to run the bGeigie devices
from Safecast. The necessary building blocks such as SD card logger, GPS
parser, power management, etc, are included in the form of an Arduino library.
The firmwares for the different devices are given as examples of this library.

# bGeigie NX

## Data log file format

The data is formatted similarly to NMEA sentences that GPS uses. It always starts with a `$`
and ends with a `*`. Following the star is a checksum. 

### Radiation data sentence

This is the basic message containing the geo-located radiation measurement.

Example:

    $BNXRDD,300,2012-12-16T17:58:24Z,31,9,115,A,4618.9996,N,00658.4623,E,587.6,A,3,77.2,1*1A
    $BNXRDD,300,2012-12-16T17:58:31Z,30,1,116,A,4618.9612,N,00658.4831,E,443.7,A,5,1.28,1*1D
    $BNXRDD,300,2012-12-16T17:58:36Z,32,4,120,A,4618.9424,N,00658.4802,E,428.1,A,6,1.27,1*18
    $BNXRDD,300,2012-12-16T17:58:41Z,32,2,122,A,4618.9315,N,00658.4670,E,425.5,A,6,1.27,1*1B
    $BNXRDD,300,2012-12-16T17:58:46Z,34,3,125,A,4618.9289,N,00658.4482,E,426.0,A,6,1.34,1*13

* Header : BNXRDD
* Device ID : Device serial number. `300`
* Date : Date formatted according to iso-8601 standard. Usually uses GMT. `2012-12-16T17:58:24Z`
* Radiation 1 minute : number of pulses given by the Geiger tube in the last minute. `31`
* Radiation 5 seconds : number of pulses given by the Geiger tube in the last 5 seconds. `31`
* Radiation total count : total number of pulses recorded since startup. `31`
* Radiation count validity flag : 'A' indicates the counter has been running for more than one minute and the 1 minute count is not zero. Otherwise, the flag is 'V' (void). `A`
* Latitude : As given by GPS. The format is `ddmm.mmmm` where `dd` is in degrees and `mm.mmmm` is decimal minute. `4618.9996`
* Hemisphere : 'N' (north), or 'S' (south). `N`
* Longitude : As given by GPS. The format is `dddmm.mmmm` where `ddd` is in degrees and `mm.mmmm` is decimal minute. `00658.4623`
* East/West : 'W' (west) or 'E' (east) from Greenwich. `E`
* Altitude : Above sea level as given by GPS in meters. `443.7`
* GPS validity : 'A' ok, 'V' invalid. `A`
* HDOP : Horizontal Dilution of Precision (HDOP), relative accuracy of horizontal position. `1.28`
* Fix Quality : 0 = invalid, 1 = GPS Fix, 2 = DGPS Fix. `1`
* Checksum. `*1D`

### Device status sentence

This is an extra sentence containing information about the sensor status.

Example:

    $BNXSTS,300,2012-12-16T17:58:24Z,4618.9996,N,00658.4623,E,v3.0.3,22,49,3987,,1,1,1*7D
    $BNXSTS,300,2012-12-16T17:58:31Z,4618.9612,N,00658.4831,E,v3.0.3,22,50,3987,,1,1,1*7F
    $BNXSTS,300,2012-12-16T17:58:36Z,4618.9424,N,00658.4802,E,v3.0.3,22,50,3987,,1,1,1*7F
    $BNXSTS,300,2012-12-16T17:58:41Z,4618.9315,N,00658.4670,E,v3.0.3,22,50,3987,,1,1,1*71
    $BNXSTS,300,2012-12-16T17:58:46Z,4618.9289,N,00658.4482,E,v3.0.3,22,49,3987,,1,1,1*75

* Header : BNXRDD
* Device ID : Device serial number. `300`
* Date : Date formatted according to iso-8601 standard. Usually uses GMT. `2012-12-16T17:58:24Z`
* Latitude : As given by GPS. The format is `ddmm.mmmm` where `dd` is in degrees and `mm.mmmm` is decimal minute. `4618.9996`
* Hemisphere : 'N' (north), or 'S' (south). `N`
* Longitude : As given by GPS. The format is `dddmm.mmmm` where `ddd` is in degrees and `mm.mmmm` is decimal minute. `00658.4623`
* East/West : 'W' (west) or 'E' (east) from Greenwich. `E`
* Firmware version number. `v3.0.3`
* Temperature in degree Celcius.
* Relative humidity in %.
* Battery voltage in millivolts.
* High voltage in volts, if available. Empty otherwise.
* SD inserted status. 1=present, 0=missing.
* SD initialization status. 1=ok, 0=failed.
* SD last write status. 1 = ok, 0 = last write failed.
* Checksum. `*7D`

### Checksum computation

The checksum is a XOR of all the ascii characters bytes between '$' and '\*' (these excluded).
Sample C code to compute the checksum is given.

    /* Compute checksum of input array */
    char gps_checksum(char *s, int N) 
    {             
      int i = 0;  
      char chk = s[0];

      for (i=1 ; i < N ; i++)                                                                
        chk ^= s[i];

      return chk;
    } 

The checksum is then always encoded as a string of two ascii characters giving the hexadecimal value of the checksum.

## Setup System on Mac OS X

### Software

* Patched Arduino IDE [download](https://github.com/downloads/fakufaku/Arduino/arduino-Safecast-20121019-macosx.zip)
* CrossPack for AVR [download](http://www.obdev.at/products/crosspack/index-de.html)
* FTDI USB-Serial drivers [x64](http://www.ftdichip.com/Drivers/VCP/MacOSX/FTDIUSBSerialDriver_v2_2_18.dmg) [x86](http://www.ftdichip.com/drivers/VCP/MacOSX/FTDIUSBSerialDriver_v2_2_18.dmg) 
  [PPC](http://www.ftdichip.com/Drivers/VCP/MacOSX/FTDIUSBSerialDriver_v2_2_18.dmg)

### Libraries

* SafecastBGeigie (this library), use branch `NextGeneration` [download](https://github.com/Safecast/SafecastBGeigie/zipball/NextGeneration) 
* chibiArduino modified to use with Atmega1284p (included with patched Arduino IDE) [download](https://github.com/fakufaku/chibiArduino)
* CmdArduino (included with patched Arduino IDE) [download](https://github.com/fakufaku/CmdArduino)
* Mighty 1284p core files (included with patched Arduino IDE) [download](https://github.com/fakufaku/mighty-1284p)
* [Option] SparkFun's Pro Micro core files, if you want to use the Atmega32u4 with a bootloader [download](http://dlnmh9ip6v2uc.cloudfront.net/datasheets/Dev/Arduino/Boards/ProMicro-Addon-190612.zip)

### Hardware

* FTDI breakout board *3.3V*, for example this [one](https://www.sparkfun.com/products/9873) or this [one](https://www.adafruit.com/products/284)
* USB male A to male mini B cable such as this [one](https://www.adafruit.com/products/260)
* A [Pocket AVR Programmer](https://www.sparkfun.com/products/9825) from SparkFun. [Using another programmer type than USBTiny type requires editing a makefile.]
* A bGeigie2 board. The [PCB design files](https://github.com/Safecast/bGeigie2) are available under CC-BY-SA 3.0 license.

## Install

### Environment

1. [Download](http://www.obdev.at/products/crosspack/index-de.html) and install the CrossPack for AVR toolchain.
2. [Download](https://github.com/downloads/fakufaku/Arduino/arduino-Safecast-20121019-macosx.zip) and install Patched Arduino IDE Arduino's website provides help [getting started](http://arduino.cc/en/Guide/MacOSX), if needed.
3. [Download](https://github.com/Safecast/SafecastBGeigie/zipball/NextGeneration) and install SafecastBGeigie library in the Arduino library folder. [Arduino libraries help](http://arduino.cc/en/Guide/Libraries).

### Upload bootloader to main microcontroller

1. Launch the Arduino-Safecast IDE
2. Connect Pocket AVR Programmer to the ISP Pin header. Pin 1 of the ISP is marked on the PCB. Make sure the switch on the programmer is set to `No power`. Make sure the battery it attached to the board and has some charge. Make sure no SD card is inserted.
  ![ISP setting](https://dl.dropbox.com/u/78009186/Photos/bGeigie3/isp_setting.jpg)
3. Pull low (off) the left switch on the small black dual switch on the board.
  ![Switch Setting 1284p](https://dl.dropbox.com/u/78009186/Photos/bGeigie3/program_1284p.jpg)
4. In Arduino IDE, select `Tools -> Programmer -> USBtinyISP`, and `Tools -> Board -> Mighty 1284p 8MHz using Optiboot`. Finally `Tools -> Burn Bootloader`.
5. Pull back up the left small switch. Both switches should be up (on) now.
  ![Switch normal mode](https://dl.dropbox.com/u/78009186/Photos/bGeigie3/normal_setting.jpg)

### Upload USB controller firmware

1. Launch the Arduino-Safecast IDE
2. Connect Pocket AVR Programmer to the ISP Pin header. Pin 1 of the ISP is marked on the PCB. Make sure the switch on the programmer is set to `No power`. Make sure the battery it attached to the board and has some charge. Make sure no SD card is inserted.
  ![ISP setting](https://dl.dropbox.com/u/78009186/Photos/bGeigie3/isp_setting.jpg)
3. Pull low (off) the small switch on the right of the black dual switch.
  ![Switch Setting 32u4](https://dl.dropbox.com/u/78009186/Photos/bGeigie3/program_32u4.jpg)
4. In a terminal, go to `SDReader32u4/LowLevelMassStorage+SD`.

        cd <arduino_libraries_path>/SDReader32u4/LowLevelMassStorage+SD

5. Compile and upload SD reader firmware to USB microcontroller. If not using a USBtinyISP kind of AVR programmer, the makefile needs to be modified before this step.

        make upload

6. Pull back up the left small switch. Both switches should be up (on) now.
  ![Switch normal mode](https://dl.dropbox.com/u/78009186/Photos/bGeigie3/normal_setting.jpg)

### Upload bGeigie3 firmware

1. Launch the Arduino-Safecast IDE
2. Plug the FTDI breakout board on the board as shown on the picture.
  [picture]
3. Select Serial port corresponding to FTDI breakout board from `Tools -> Serial Port -> [my port]`.
4. Open the bGeigie3 sketch from menu `Examples->SafecastBGeigie->bGeigie3`.
5. Press upload button (or `File -> Upload`).
6. Open the Serial terminal by pressing button (or `Tools -> Serial Monitor`), you should see something like

        *** Welcome to bGeigie ***
        Version-3.0.1
        GPS start time,590ms
        --- Diagnostic START ---
        Version,3.0.1
        Device ID,300
        Radio initialized,yes
        Radio address,3300
        Radio channel,20
        GPS type MTK,yes
        GPS system startup,yes
        SD inserted,yes
        SD initialized,yes
        SD open file,yes
        SD read write,yes
        SD reader initialized,yes
        Temperature,24C
        Humidity,49%
        Battery voltage,3827mV
        System free RAM,13507B
        --- Diagnostic END ---
        Starting now!
        bGeigie sleeps... good night.

    Congratulations, we're half-way there.

## Example Sketches

The sketches given in examples are the actuall firmware of the different Safecast bGeigie devices:

* bGeigieMini
* bGeigieClassic
* bGeigieNinja
* bGeigieConfigBurner
* SlidingWindowCounter

## License

    Copyright (c) 2011-2012, Robin Scheibler aka FakuFaku, Christopher Wang aka Akiba
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. Neither the name of the <organization> nor the
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



