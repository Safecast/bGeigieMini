### Safecast bGeigie Library

This library implements the basic blocks used in the bGeigie system from Safecast.

Following classes are included:

* GPS: Simple class reading geo-location data received from a GPS module connected through the Hardware Serial port.
* HardwareCounter: Uses the Timer1 of the 328p as a hardware counter to record the number of pulses in a given time interval.
* InterruptCounter: A small library using the hardware interrupt as an event counter.

### Examples

The sketches given in examples are the actuall firmware of the different Safecast bGeigie devices:

* bGeigieMini
* bGeigieClassic
* bGeigieNinja
* bGeigieConfigBurner
* SlidingWindowCounter

### Dependencies

* chibiArduino modified to use with Atmega1284p ([here](https://github.com/fakufaku/chibiArduino)
* CmdArduino [here](https://github.com/fakufaku/CmdArduino)
* Mighty 1284p core files [here](https://github.com/fakufaku/mighty-1284p)
* SparkFun's Pro Micro core files, if you want to use the Atmega32u4 with a bootloader [here](http://dlnmh9ip6v2uc.cloudfront.net/datasheets/Dev/Arduino/Boards/ProMicro-Addon-190612.zip)

### Usage

1) Download and copy to the Arduino library folder SafecastBGeigie, chibiArduino, and CmdArduino
2) If not present, create a `hardware` folder, at the same level as the `library` folder and copy the Mighty1284p and ProMicro core files
3) Open arduino and look for the examples