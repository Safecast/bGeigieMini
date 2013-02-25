/*
             LUFA Library
     Copyright (C) Dean Camera, 2012.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2012  Dean Camera (dean [at] fourwalledcubicle [dot] com)
        (c) 2013  Robin Scheibler, Safecast (fakufaku [at] gmail [dot] com)

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
 *  Main source file for the Mass Storage demo. This file contains the main tasks of the demo and
 *  is responsible for the initial application hardware configuration.
 */

#define  INCLUDE_FROM_MASSSTORAGE_C
#include "MassStorage.h"
#include "version.h"

#include <avr/power.h>
#include <avr/sleep.h>
#include "Lib/sd_raw_config.h"
#include "Lib/Timer.h"

#include <LUFA/Drivers/Peripheral/Serial.h>

/** Structure to hold the latest Command Block Wrapper issued by the host, containing a SCSI command to execute. */
MS_CommandBlockWrapper_t  CommandBlock;

/** Structure to hold the latest Command Status Wrapper to return to the host, containing the status of the last issued command. */
MS_CommandStatusWrapper_t CommandStatus = { .Signature = MS_CSW_SIGNATURE };

/** Flag to asynchronously abort any in-progress data transfers upon the reception of a mass storage reset command. */
volatile bool IsMassStoreReset = false;

/** A state variable to enable mass storage when connected **/
#define IDLE 0
#define CONNECTED 1
static int state = IDLE;

static int SDCardManager_init_flag = 0;

/** Main program entry point. This routine configures the hardware required by the application, then
 *  enters a loop to run the application tasks in sequence.
 */
int main(void)
{
  // turn on interrupt
  sei();

  // setup hardware
	SetupHardware();

	for (;;)
	{
    if (state == CONNECTED)
    {
      // initialize the card manager if needed
      if (SDCardManager_init_flag == 0)
      {
        SDCardManager_Init();
        SDCardManager_init_flag = 1;
      }
      
      // perform necessary tasks
      MassStorage_Task();
      USB_USBTask();
    }
    else
    {
      GoToSleep();
    }
	}
}

/** Configures the board hardware and chip peripherals for the demo's functionality. */
void SetupHardware(void)
{
  int i;
	/* Disable watchdog if enabled by bootloader/fuses */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	/* Disable clock division */
	clock_prescale_set(clock_div_1);

  // init timer
  timer_init();

	/* Hardware Initialization */
  LED_init();

  // Setup serial stream
	Serial_Init(9600, false);
  Serial_CreateStream(NULL);
  
  // Welcome!
  printf("Welcome to bGeigie MassStorage Version-%s\n", version);

  /* configure IRQ pin and set low */
  configure_pin_irq();
  irq_low();

  // set the SD card manager to uninitialized
  SDCardManager_init_flag = 0;

  // wait a little
  for (i = 0 ; i < 10 ; i++)
  {
    LED_on();
    delay(250);
    LED_off();
    delay(250);
  }

  // Init USB
	USB_Init();
}

/** Configure AVR for sleep */
void GoToSleep(void)
{
  printf("Sleep\r\n");
  delay(10);

  SDCardManager_init_flag = 0;

  ADCSRA &= ~(1<<ADEN); //Disable ADC                                                    
  ACSR |= (1<<ACD); //Disable the analog comparator                                      
  //Disable digital input buffers on all ADC0-ADC5 pins                    
  DIDR0 = 0xFF; 
  DIDR1 = 0xFF;
  DIDR2 = 0xFF;

  // set MISO pin as input
  DDRB &= ~(1 << DDB3);

  power_twi_disable();
  power_spi_disable();
  power_usart0_disable();                                                                
  power_timer0_disable();
  power_timer1_disable();
  power_timer3_disable();

  //Power down various bits of hardware to lower power usage                             
  //SMCR |= ~(1 << SM2) | (1 << SM1) | ~(1 << SM0); // power-down sleep mode
  //SMCR |= (1 << SE); // SLEEP!
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_mode();

  /*********/
  /* SLEEP */
  /*********/

  /* Processor wakes up here */

  //Enable ADC, TWI, SPI, Timer0, Timer1, Timer2
  ADCSRA |= (1<<ADEN); // Enable ADC
  ACSR &= ~(1<<ACD);   // Enable the analog comparator

  // this should be set to reflect real usage of analog pins
  DIDR0 = 0x00;   // Enable digital input buffers on all ADC0-ADC7 pins
  DIDR1 = 0x00; // Enable digital input buffer on AIN1/0
  DIDR2 = 0x00; // Enable digital input buffer on ADC8-ADC13
  
  // set MISO pin as input
  DDRB &= ~(1 << DDB3);

  power_twi_enable();
  power_spi_enable();
  power_usart0_enable();
  power_timer0_enable();
  power_timer1_enable();
  power_timer3_enable();

  printf("Wake Up!\r\n");
}

/** Event handler for the USB_Connect event. This indicates that the device is enumerating via the status LEDs. */
void EVENT_USB_Device_Connect(void)
{
  // switch to CONNECTED state
  state = CONNECTED;

	/* Indicate USB enumerating */
	//LED_on();

	/* Reset the MSReset flag upon connection */
	IsMassStoreReset = false;
}

/** Event handler for the USB_Disconnect event. This indicates that the device is no longer connected to a host via
 *  the status LEDs and stops the Mass Storage management task.
 */
void EVENT_USB_Device_Disconnect(void)
{
	/* Indicate USB not ready */
	LED_off();
  
  // switch to idle state
  state = IDLE;
}

/** Event handler for the USB_ConfigurationChanged event. This is fired when the host set the current configuration
 *  of the USB device after enumeration - the device endpoints are configured and the Mass Storage management task started.
 */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;

	/* Setup Mass Storage Data Endpoints */
	ConfigSuccess &= Endpoint_ConfigureEndpoint(MASS_STORAGE_IN_EPADDR,  EP_TYPE_BULK, MASS_STORAGE_IO_EPSIZE, 1);
	ConfigSuccess &= Endpoint_ConfigureEndpoint(MASS_STORAGE_OUT_EPADDR, EP_TYPE_BULK, MASS_STORAGE_IO_EPSIZE, 1);

	/* Indicate endpoint configuration success or failure */
	// put optional LED code here depending on value of ConfigSuccess
}

/** Event handler for the USB_ControlRequest event. This is used to catch and process control requests sent to
 *  the device from the USB host before passing along unhandled control requests to the library for processing
 *  internally.
 */
void EVENT_USB_Device_ControlRequest(void)
{
	/* Process UFI specific control requests */
	switch (USB_ControlRequest.bRequest)
	{
		case MS_REQ_MassStorageReset:
			if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				Endpoint_ClearSETUP();
				Endpoint_ClearStatusStage();

				/* Indicate that the current transfer should be aborted */
				IsMassStoreReset = true;
			}

			break;
		case MS_REQ_GetMaxLUN:
			if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				Endpoint_ClearSETUP();

				/* Indicate to the host the number of supported LUNs (virtual disks) on the device */
				Endpoint_Write_8(TOTAL_LUNS - 1);

				Endpoint_ClearIN();
				Endpoint_ClearStatusStage();
			}

			break;
	}
}

/** Task to manage the Mass Storage interface, reading in Command Block Wrappers from the host, processing the SCSI commands they
 *  contain, and returning Command Status Wrappers back to the host to indicate the success or failure of the last issued command.
 */
void MassStorage_Task(void)
{
	/* Device must be connected and configured for the task to run */
	if (USB_DeviceState != DEVICE_STATE_Configured)
	  return;

	/* Process sent command block from the host if one has been sent */
	if (ReadInCommandBlock())
	{
		/* Indicate busy */
		// Optional LED blinking can go here 

		/* Check direction of command, select Data IN endpoint if data is from the device */
		if (CommandBlock.Flags & MS_COMMAND_DIR_DATA_IN)
		  Endpoint_SelectEndpoint(MASS_STORAGE_IN_EPADDR);

		/* Decode the received SCSI command, set returned status code */
		CommandStatus.Status = SCSI_DecodeSCSICommand() ? MS_SCSI_COMMAND_Pass : MS_SCSI_COMMAND_Fail;

		/* Load in the CBW tag into the CSW to link them together */
		CommandStatus.Tag = CommandBlock.Tag;

		/* Load in the data residue counter into the CSW */
		CommandStatus.DataTransferResidue = CommandBlock.DataTransferLength;

		/* Stall the selected data pipe if command failed (if data is still to be transferred) */
		if ((CommandStatus.Status == MS_SCSI_COMMAND_Fail) && (CommandStatus.DataTransferResidue))
		  Endpoint_StallTransaction();

		/* Return command status block to the host */
		ReturnCommandStatus();

		/* Indicate ready */
		// Optional LED blinking can go here 
	}

	/* Check if a Mass Storage Reset occurred */
	if (IsMassStoreReset)
	{
		/* Reset the data endpoint banks */
		Endpoint_ResetEndpoint(MASS_STORAGE_OUT_EPADDR);
		Endpoint_ResetEndpoint(MASS_STORAGE_IN_EPADDR);

		Endpoint_SelectEndpoint(MASS_STORAGE_OUT_EPADDR);
		Endpoint_ClearStall();
		Endpoint_ResetDataToggle();
		Endpoint_SelectEndpoint(MASS_STORAGE_IN_EPADDR);
		Endpoint_ClearStall();
		Endpoint_ResetDataToggle();

		/* Clear the abort transfer flag */
		IsMassStoreReset = false;
	}
}

/** Function to read in a command block from the host, via the bulk data OUT endpoint. This function reads in the next command block
 *  if one has been issued, and performs validation to ensure that the block command is valid.
 *
 *  \return Boolean true if a valid command block has been read in from the endpoint, false otherwise
 */
static bool ReadInCommandBlock(void)
{
	uint16_t BytesTransferred;

	/* Select the Data Out endpoint */
	Endpoint_SelectEndpoint(MASS_STORAGE_OUT_EPADDR);

	/* Abort if no command has been sent from the host */
	if (!(Endpoint_IsOUTReceived()))
	  return false;

	/* Read in command block header */
	BytesTransferred = 0;
	while (Endpoint_Read_Stream_LE(&CommandBlock, (sizeof(CommandBlock) - sizeof(CommandBlock.SCSICommandData)),
	                               &BytesTransferred) == ENDPOINT_RWSTREAM_IncompleteTransfer)
	{
		/* Check if the current command is being aborted by the host */
		if (IsMassStoreReset)
		  return false;
	}

	/* Verify the command block - abort if invalid */
	if ((CommandBlock.Signature         != MS_CBW_SIGNATURE) ||
	    (CommandBlock.LUN               >= TOTAL_LUNS)       ||
		(CommandBlock.Flags              & 0x1F)             ||
		(CommandBlock.SCSICommandLength == 0)                ||
		(CommandBlock.SCSICommandLength >  sizeof(CommandBlock.SCSICommandData)))
	{
		/* Stall both data pipes until reset by host */
		Endpoint_StallTransaction();
		Endpoint_SelectEndpoint(MASS_STORAGE_IN_EPADDR);
		Endpoint_StallTransaction();

		return false;
	}

	/* Read in command block command data */
	BytesTransferred = 0;
	while (Endpoint_Read_Stream_LE(&CommandBlock.SCSICommandData, CommandBlock.SCSICommandLength,
	                               &BytesTransferred) == ENDPOINT_RWSTREAM_IncompleteTransfer)
	{
		/* Check if the current command is being aborted by the host */
		if (IsMassStoreReset)
		  return false;
	}

	/* Finalize the stream transfer to send the last packet */
	Endpoint_ClearOUT();

	return true;
}

/** Returns the filled Command Status Wrapper back to the host via the bulk data IN endpoint, waiting for the host to clear any
 *  stalled data endpoints as needed.
 */
static void ReturnCommandStatus(void)
{
	uint16_t BytesTransferred;

	/* Select the Data Out endpoint */
	Endpoint_SelectEndpoint(MASS_STORAGE_OUT_EPADDR);

	/* While data pipe is stalled, wait until the host issues a control request to clear the stall */
	while (Endpoint_IsStalled())
	{
		/* Check if the current command is being aborted by the host */
		if (IsMassStoreReset)
		  return;
	}

	/* Select the Data In endpoint */
	Endpoint_SelectEndpoint(MASS_STORAGE_IN_EPADDR);

	/* While data pipe is stalled, wait until the host issues a control request to clear the stall */
	while (Endpoint_IsStalled())
	{
		/* Check if the current command is being aborted by the host */
		if (IsMassStoreReset)
		  return;
	}

	/* Write the CSW to the endpoint */
	BytesTransferred = 0;
	while (Endpoint_Write_Stream_LE(&CommandStatus, sizeof(CommandStatus),
	                                &BytesTransferred) == ENDPOINT_RWSTREAM_IncompleteTransfer)
	{
		/* Check if the current command is being aborted by the host */
		if (IsMassStoreReset)
		  return;
	}

	/* Finalize the stream transfer to send the last packet */
	Endpoint_ClearIN();
}

