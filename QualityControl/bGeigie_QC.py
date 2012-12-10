
import serial
import io
import sys
import re

from time import clock

import os
# chose an implementation, depending on os
if os.name == 'posix':
  from serial.tools.list_ports_posix import *
#elif os.name == 'nt': #sys.platform == 'win32':
  #from serial.tools.list_ports_windows import *
#~ elif os.name == 'java':
else:
  raise ImportError("Sorry: no implementation for your platform ('%s') available" % (os.name,))

# bGeigie diagnostic object

class BGeigieDiagnostic:
  # general parameters
  version = None
  ID = None
  # radio
  RadioEnabled = False
  RadioInit = False
  # gps
  GPSTypeMTK = False
  GPSStart = False
  # SD
  SDInserted = False
  SDInit = False
  SDOpen = False
  SDRW = False
  # SD Reader
  SDReaderEnabled = False
  SDReaderInit = False
  # Features
  PwrManageEnabled = False
  CmdLineIntEnabled = False
  CoordTruncEnabled = False
  # environment
  Temperature = 0
  Humidity = 0
  BatteryVoltage = 0
  FreeRam = 0

  # Diagnostic analysis method
  def diagnosticAnalysis(self):
    print "DIAGNOSTIC"

    print "  Radio : ",
    if (not self.RadioEnabled or not self.RadioInit):
      radio_success = False
      print "failed ",
      if (not self.RadioEnabled):
        print "(cause : Radio not enabled)"
      elif (not self.RadioInit):
        print "(cause : Radio fails initialization)"
      else:
        print "(cause : unknown)"
    else:
      radio_success = True
      print "success"

    print "  GPS : ",
    if (not self.GPSTypeMTK or not self.GPSStart):
      gps_success = False
      print "failed ",
      if (not self.GPSTypeMTK):
        print "(cause : Wrong GPS type (not MTK))"
      elif (not self.GPSStart):
        print "(cause : GPS Start test fails)"
      else:
        print "(cause : unknown)"
    else:
      gps_success = True
      print "success"

    print "  SD card : ",
    if (not self.SDInserted or not self.SDInit or not self.SDOpen or not self.SDRW):
      sd_success = False
      print "failed ",
      if (not self.SDInserted):
        print "(cause : SD card not inserted)"
      elif (not self.SDInit):
        print "(cause : SD card initialization fails)"
      elif (not self.SDOpen):
        print "(cause : File open test fails)"
      elif (not self.SDRW):
        print "(cause : Read/Write test fails)"
      else:
        print "(cause : unknown)"
    else:
      sd_success = True
      print "success"

    print "  SD Reader : ",
    if (not self.SDReaderEnabled or not self.SDInit):
      sdreader_success = False
      print "failed ",
      if (not self.SDReaderEnabled):
        print "(cause : SD Reader not enabled)"
      elif (not self.SDInit):
        print "(cause : SD Reader initialization fails)"
      else:
        print "(cause : unknown)"
    else:
      sdreader_success = True
      print "success"

    print "  Required features : ",
    if (not self.PwrManageEnabled or not self.CmdLineIntEnabled or not self.CoordTruncEnabled):
      features_success = False
      print "failed"
      if (not self.PwrManageEnabled):
        print "    Power Management required"
      if (not self.CmdLineIntEnabled):
        print "    Command Line Interface required"
      if (not self.CoordTruncEnabled):
        print "    Coordination Truncation required"
    else:
      features_success = True
      print "success"

    if (radio_success and sd_success and sdreader_success and gps_success and features_success):
      print "Result : bGeigie ready for operation."
    else:
      print "Result : bGeigie faulty."


########
# MAIN #
########

def usage():
  print 'Usage: ' + sys.argv[0] + ' -p <port> [-b <baudrate>]'

# default arguments
port = ''
baudrate = 57600
timeout = 10

# parse arguments
#n = 1
#while (n < len(sys.argv)):
  #if (sys.argv[n] == '-p'):
    #port = sys.argv[n+1]
    #n += 2
  #elif (sys.argv[n] == '-b'):
    #baudrate = int(sys.argv[n+1])
    #n += 2
  #else:
    #usage()
    #sys.exit(1)

re_port = re.compile('^.*usbserial-[A-Za-z0-9]{8}')

hits = 0
iterator = sorted(comports())
for p, desc, hwid in iterator:
  m_port = re_port.match(p)
  if m_port:
    port = desc
    break

if port == '':
  print "No matching port found."
  sys.exit(0)
else:
  print "Found matching port '%s'. Try to open." % (desc,)

# try to open serial port
try:
  ser = serial.Serial(port, baudrate, timeout=1)
except ValueError:
  print 'Wrong serial parameters. Exiting.'
  sys.exit(1)
except serial.SerialException:
  print 'Device can not be found or can not be configured.'
  sys.exit(1)

start = clock()
gotStart = False
success = False
bgd = BGeigieDiagnostic()

# read Lines
while (not success and clock() - start < timeout):
  line = ser.readline()
  line = line[:-2] # strip \r\n at end of line

  if (line == "--- Diagnostic START ---"):
    gotStart = True
    start = clock()
    continue

  if (gotStart and line == "--- Diagnostic END ---"):
    success = True
    break

  if (gotStart):
    start = clock()         # reset timeout
    diag = line.split(',')  # split at comma

    if (diag[0] == "Version"):
      bgd.version = diag[1]
    elif (diag[0] == "Device ID"):
      bgd.ID = diag[1]

    elif (diag[0] == "Radio enabled"):
      if (diag[1] == "yes"):
        bgd.RadioEnabled = True
    elif (diag[0] == "Radio initialized"):
      if (diag[1] == "yes"):
        bgd.RadioInit = True

    elif (diag[0] == "GPS type MTK"):
      if (diag[1] == "yes"):
        bgd.GPSTypeMTK = True
    elif (diag[0] == "GPS system startup"):
      if (diag[1] == "yes"):
        bgd.GPSStart = True

    elif (diag[0] == "SD inserted"):
      if (diag[1] == "yes"):
        bgd.SDInserted = True
    elif (diag[0] == "SD initialized"):
      if (diag[1] == "yes"):
        bgd.SDInit = True
    elif (diag[0] == "SD open file"):
      if (diag[1] == "yes"):
        bgd.SDOpen = True
    elif (diag[0] == "SD read write"):
      if (diag[1] == "yes"):
        bgd.SDRW = True

    elif (diag[0] == "SD reader enabled"):
      if (diag[1] == "yes"):
        bgd.SDReaderEnabled = True
    elif (diag[0] == "SD reader initialized"):
      if (diag[1] == "yes"):
        bgd.SDReaderInit = True

    elif (diag[0] == "Power management enabled"):
      if (diag[1] == "yes"):
        bgd.PwrManageEnabled = True

    elif (diag[0] == "Command line interface enabled"):
      if (diag[1] == "yes"):
        bgd.CmdLineIntEnabled = True

    elif (diag[0] == "Coordinate truncation enabled"):
      if (diag[1] == "yes"):
        bgd.CoordTruncEnabled = True

    elif (diag[0] == "Temperature"):
      try:
        bgd.Temperature = int(diag[1][:-1])
      except ValueError:
        print 'Warning : unrecognized temperature format.'
    elif (diag[0] == "Humidity"):
      try:
        bgd.Humidity = int(diag[1][:-1])
      except ValueError:
        print 'Warning : unrecognized humidity format.'
    elif (diag[0] == "Battery voltage"):
      try:
        bgd.BatteryVoltage = int(diag[1][:-2])
      except ValueError:
        print 'Warning : unrecognized battery voltage format.'
    elif (diag[0] == "System free RAM"):
      try:
        bgd.FreeRam = int(diag[1][:-1])
      except ValueError:
        print 'Warning : unrecognized free RAM format.'
    else:
      continue
      

if (success):
  bgd.diagnosticAnalysis()
else:
  print "Timeout : failed to perform diagnostic."
  sys.exit(1)



