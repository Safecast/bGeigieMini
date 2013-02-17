/*
             LUFA Library
     Copyright (C) Dean Camera, 2012.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2012  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaim all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  Header file for MassStorage.c.
 */

#ifndef _MASS_STORAGE_H_
#define _MASS_STORAGE_H_

	/* Includes: */
		#include <avr/io.h>
		#include <avr/wdt.h>
		#include <avr/power.h>
		#include <avr/interrupt.h>

		#include "Descriptors.h"

		#include "Lib/SCSI.h"
		#include "Lib/SDCardManager.h"
		#include "Config/AppConfig.h"
		#include "Config/LEDs.h"

		#include <LUFA/Drivers/USB/USB.h>
		#include <LUFA/Drivers/Peripheral/Serial.h>

	/* Macros: */
		/** Mass Storage Class specific request to reset the Mass Storage interface, ready for the next command. */
		#define REQ_MassStorageReset       0xFF

		/** Mass Storage Class specific request to retrieve the total number of Logical Units (drives) in the SCSI device. */
		#define REQ_GetMaxLUN              0xFE

		/** Maximum length of a SCSI command which can be issued by the device or host in a Mass Storage bulk wrapper. */
		#define MAX_SCSI_COMMAND_LENGTH    16
		
		/** Total number of Logical Units (drives) in the device. The total device capacity is shared equally between
		 *  each drive - this can be set to any positive non-zero amount.
		 */
		#define TOTAL_LUNS                 1
		
		/** Blocks in each LUN, calculated from the total capacity divided by the total number of Logical Units in the device. */
		#define LUN_MEDIA_BLOCKS           (SDCardManager_GetNbBlocks() / TOTAL_LUNS)    
		
		/** Magic signature for a Command Block Wrapper used in the Mass Storage Bulk-Only transport protocol. */
		#define CBW_SIGNATURE              0x43425355UL

		/** Magic signature for a Command Status Wrapper used in the Mass Storage Bulk-Only transport protocol. */
		#define CSW_SIGNATURE              0x53425355UL
		
		/** Mask for a Command Block Wrapper's flags attribute to specify a command with data sent from host-to-device. */
		#define COMMAND_DIRECTION_DATA_OUT (0 << 7)

		/** Mask for a Command Block Wrapper's flags attribute to specify a command with data sent from device-to-host. */
		#define COMMAND_DIRECTION_DATA_IN  (1 << 7)

		/** LED mask for the library LED driver, to indicate that the USB interface is not ready. */
		#define LEDMASK_USB_NOTREADY       LEDS_LED1

		/** LED mask for the library LED driver, to indicate that the USB interface is enumerating. */
		#define LEDMASK_USB_ENUMERATING   (LEDS_LED2 | LEDS_LED3)

		/** LED mask for the library LED driver, to indicate that the USB interface is ready. */
		#define LEDMASK_USB_READY         (LEDS_LED2 | LEDS_LED4)

		/** LED mask for the library LED driver, to indicate that an error has occurred in the USB interface. */
		#define LEDMASK_USB_ERROR         (LEDS_LED1 | LEDS_LED3)

		/** LED mask for the library LED driver, to indicate that the USB interface is busy. */
		#define LEDMASK_USB_BUSY           LEDS_LED2

	/* Global Variables: */
		extern MS_CommandBlockWrapper_t  CommandBlock;
		extern MS_CommandStatusWrapper_t CommandStatus;
		extern volatile bool             IsMassStoreReset;

	/* Function Prototypes: */
		void SetupHardware(void);
    void GoToSleep(void);
		void MassStorage_Task(void);

		void EVENT_USB_Device_Connect(void);
		void EVENT_USB_Device_Disconnect(void);
		void EVENT_USB_Device_ConfigurationChanged(void);
		void EVENT_USB_Device_ControlRequest(void);

		#if defined(INCLUDE_FROM_MASSSTORAGE_C)
			static bool ReadInCommandBlock(void);
			static void ReturnCommandStatus(void);
		#endif

#endif

