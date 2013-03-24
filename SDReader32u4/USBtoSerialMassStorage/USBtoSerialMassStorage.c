/*
             LUFA Library
     Copyright (C) Dean Camera, 2012.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2012  Dean Camera (dean [at] fourwalledcubicle [dot] com)
  Modified 2013 Robin Scheibler (robin.scheibler [at] gmail [dot] com)

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
 *  Main source file for the USBSerialMassStorage firmware.
 *  This firmware is used on the bGeigie3 (aka NX) device
 *  for Safecast Japan.
 */

#include "USBtoSerialMassStorage.h"

#include <avr/power.h>
#include <avr/sleep.h>
#include "Lib/sd_raw_config.h"
#include "Lib/Timer.h"

#include <LUFA/Drivers/Peripheral/Serial.h>
#include <LUFA/Drivers/Misc/RingBuffer.h>

/** LUFA CDC Class driver interface configuration and state information. This structure is
 *  passed to all CDC Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface =
	{
		.Config =
			{
				.ControlInterfaceNumber         = 0,
				.DataINEndpoint                 =
					{
						.Address                = CDC_TX_EPADDR,
						.Size                   = CDC_TXRX_EPSIZE,
						.Banks                  = 1,
					},
				.DataOUTEndpoint                =
					{
						.Address                = CDC_RX_EPADDR,
						.Size                   = CDC_TXRX_EPSIZE,
						.Banks                  = 1,
					},
				.NotificationEndpoint           =
					{
						.Address                = CDC_NOTIFICATION_EPADDR,
						.Size                   = CDC_NOTIFICATION_EPSIZE,
						.Banks                  = 1,
					},
			},
	};

/** LUFA Mass Storage Class driver interface configuration and state information. This structure is
 *  passed to all Mass Storage Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_MS_Device_t Disk_MS_Interface =
	{
		.Config =
			{
				.InterfaceNumber                = 2,
				.DataINEndpoint                 =
					{
						.Address                = MASS_STORAGE_IN_EPADDR,
						.Size                   = MASS_STORAGE_IO_EPSIZE,
						.Banks                  = 1,
					},
				.DataOUTEndpoint                =
					{
						.Address                = MASS_STORAGE_OUT_EPADDR,
						.Size                   = MASS_STORAGE_IO_EPSIZE,
						.Banks                  = 1,
					},
				.TotalLUNs                      = TOTAL_LUNS,
			},
	};

/** Standard file stream for the CDC interface when set up, so that the virtual CDC COM port can be
 *  used like any regular character stream in the C APIs
 */
static FILE USBSerialStream;

/** Circular buffer to hold data from the host before it is sent to the device via the serial port. */
static RingBuffer_t USBtoUSART_Buffer;

/** Underlying data buffer for \ref USBtoUSART_Buffer, where the stored bytes are located. */
static uint8_t      USBtoUSART_Buffer_Data[128];

/** Circular buffer to hold data from the serial port before it is sent to the host. */
static RingBuffer_t USARTtoUSB_Buffer;

/** Underlying data buffer for \ref USARTtoUSB_Buffer, where the stored bytes are located. */
static uint8_t      USARTtoUSB_Buffer_Data[128];

/** A state variable to enable mass storage when connected **/
#define IDLE 0
#define CONNECTED 1
static int state = IDLE;
static int SDCardManager_init_flag = 0;

/** Main program entry point. This routine contains the overall program flow, including initial
 *  setup of all components and the main program loop.
 */
int main(void)
{
  // setup the hardware
	SetupHardware();

  // initialize ring buffer
	RingBuffer_InitBuffer(&USBtoUSART_Buffer, USBtoUSART_Buffer_Data, sizeof(USBtoUSART_Buffer_Data));
	RingBuffer_InitBuffer(&USARTtoUSB_Buffer, USARTtoUSB_Buffer_Data, sizeof(USARTtoUSB_Buffer_Data));

	/* Create a regular character stream for the interface so that it can be used with the stdio.h functions */
	CDC_Device_CreateStream(&VirtualSerial_CDC_Interface, &USBSerialStream);

  // turn on interrupt
  sei();

  // wait a little if the restart wasn't from the watchdog timer
  if (!(MCUSR & _BV(WDRF)))
  {
    for (int i = 0 ; i < 10 ; i++)
    {
      LED_on();
      delay(250);
      LED_off();
      delay(250);
    }
  }
  else
  {
    // clear the watchdog timer reset flag
    MCUSR &= ~_BV(WDRF);
  }

  // Start the main loop
	for (;;)
	{
    if (state == CONNECTED)
    {
      // initialize the card manager if necessary
      if (SDCardManager_init_flag == 0)
      {
        SDCardManager_Init();
        SDCardManager_init_flag = 1;
      }

      /* Must throw away unused bytes from the host, or it will lock up while waiting for the device */
      //CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);

      /***********************************/
      /** Start USB to USART processing **/
      /***********************************/

      /* Only try to read in bytes from the CDC interface if the transmit buffer is not full */
      if (!(RingBuffer_IsFull(&USBtoUSART_Buffer)))
      {
        int16_t ReceivedByte = CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);

        /* Read bytes from the USB OUT endpoint into the USART transmit buffer */
        if (!(ReceivedByte < 0))
          RingBuffer_Insert(&USBtoUSART_Buffer, ReceivedByte);
      }

      /* Check if the UART receive buffer flush timer has expired or the buffer is nearly full */
      uint16_t BufferCount = RingBuffer_GetCount(&USARTtoUSB_Buffer);
      if ((TIFR4 & (1 << TOV4)) || (BufferCount > (uint8_t)(sizeof(USARTtoUSB_Buffer_Data) * .75)))
      {
        /* Clear flush timer expiry flag */
        TIFR4 |= (1 << TOV4);

        /* Read bytes from the USART receive buffer into the USB IN endpoint */
        while (BufferCount--)
        {
          /* Try to send the next byte of data to the host, abort if there is an error without dequeuing */
          if (CDC_Device_SendByte(&VirtualSerial_CDC_Interface,
                RingBuffer_Peek(&USARTtoUSB_Buffer)) != ENDPOINT_READYWAIT_NoError)
          {
            break;
          }

          /* Dequeue the already sent byte from the buffer now we have confirmed that no transmission error occurred */
          RingBuffer_Remove(&USARTtoUSB_Buffer);
        }
      }

      /* Load the next byte from the USART transmit buffer into the USART */
      if (!(RingBuffer_IsEmpty(&USBtoUSART_Buffer)))
        Serial_SendByte(RingBuffer_Remove(&USBtoUSART_Buffer));

      /*********************************/
      /** Finished USB to USART tasks **/
      /*********************************/


      CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
      MS_Device_USBTask(&Disk_MS_Interface);
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
	/* Disable watchdog if enabled by bootloader/fuses */
	wdt_disable();

	/* Disable clock division */
	clock_prescale_set(clock_div_1);

  // init timer
  timer_init();

	/* Hardware Initialization */
	LED_init();

  // Setup serial stream
	//Serial_Init(9600, false);
  //Serial_CreateStream(NULL);
  
  /* configure IRQ pin and set low */
  configure_pin_irq();
  irq_low();

  // set the SD card manager to uninitialized
  SDCardManager_init_flag = 0;

  // Init USB
	USB_Init();

	/* Start the flush timer so that overflows occur rapidly to push received bytes to the USB interface */
	TCCR4B = (1 << CS43); // clk/128 = 62.5kHz
  OCR4C = 0xFF;         // Set the TOP (max) value of couter (8bit)
	
}

/** Configure AVR for sleep */
void GoToSleep(void)
{
  //printf("Sleep\r\n");
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

  //printf("Wake Up!\r\n");
}

/** Example function to write to Serial stream */
void WriteUSBStream(char *string)
{
	char*       ReportString  = NULL;

	if (ReportString != NULL)
	{
		/* Write the string to the virtual COM port via the created character stream */
		fputs(ReportString, &USBSerialStream);

		/* Alternatively, without the stream: */
		// CDC_Device_SendString(&VirtualSerial_CDC_Interface, ReportString);
	}
}

// Example of echo serial data
void EchoSerial(void)
{
  // Echo on virtual serial
  if (CDC_Device_BytesReceived(&VirtualSerial_CDC_Interface)) 
  { 
    CDC_Device_SendByte(&VirtualSerial_CDC_Interface, CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface)); 
  } 
}


/** Event handler for the library USB Connection event. */
void EVENT_USB_Device_Connect(void)
{
  // switch to CONNECTED state
  state = CONNECTED;

	/* Indicate USB enumerating */
	LED_on();

	/* Reset the MSReset flag upon connection */
  //MSInterfaceInfo->State.IsMassStoreReset = false;
}

/** Event handler for the library USB Disconnection event. */
void EVENT_USB_Device_Disconnect(void)
{
	/* Indicate USB not ready */
	LED_off();
  
  // switch to idle state
  state = IDLE;
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;

	ConfigSuccess &= CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface);
	ConfigSuccess &= MS_Device_ConfigureEndpoints(&Disk_MS_Interface);

	//LEDs_SetAllLEDs(ConfigSuccess ? LEDMASK_USB_READY : LEDMASK_USB_ERROR);
}

/** Event handler for the library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void)
{
	CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
	MS_Device_ProcessControlRequest(&Disk_MS_Interface);
}

/** Mass Storage class driver callback function the reception of SCSI commands from the host, which must be processed.
 *
 *  \param[in] MSInterfaceInfo  Pointer to the Mass Storage class interface configuration structure being referenced
 */
bool CALLBACK_MS_Device_SCSICommandReceived(USB_ClassInfo_MS_Device_t* const MSInterfaceInfo)
{
	bool CommandSuccess;

	//LEDs_SetAllLEDs(LEDMASK_USB_BUSY);
	CommandSuccess = SCSI_DecodeSCSICommand(MSInterfaceInfo);
	//LEDs_SetAllLEDs(LEDMASK_USB_READY);

	return CommandSuccess;
}

/** ISR to manage the reception of data from the serial port, placing received bytes into a circular buffer
 *  for later transmission to the host.
 */
ISR(USART1_RX_vect, ISR_BLOCK)
{
	uint8_t ReceivedByte = UDR1;

	if (USB_DeviceState == DEVICE_STATE_Configured)
	  RingBuffer_Insert(&USARTtoUSB_Buffer, ReceivedByte);
}

/** Event handler for the CDC Class driver Line Encoding Changed event.
 *
 *  \param[in] CDCInterfaceInfo  Pointer to the CDC class interface configuration structure being referenced
 */
void EVENT_CDC_Device_LineEncodingChanged(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo)
{
	uint8_t ConfigMask = 0;

	switch (CDCInterfaceInfo->State.LineEncoding.ParityType)
	{
		case CDC_PARITY_Odd:
			ConfigMask = ((1 << UPM11) | (1 << UPM10));
			break;
		case CDC_PARITY_Even:
			ConfigMask = (1 << UPM11);
			break;
	}

	if (CDCInterfaceInfo->State.LineEncoding.CharFormat == CDC_LINEENCODING_TwoStopBits)
	  ConfigMask |= (1 << USBS1);

	switch (CDCInterfaceInfo->State.LineEncoding.DataBits)
	{
		case 6:
			ConfigMask |= (1 << UCSZ10);
			break;
		case 7:
			ConfigMask |= (1 << UCSZ11);
			break;
		case 8:
			ConfigMask |= ((1 << UCSZ11) | (1 << UCSZ10));
			break;
	}

	/* Must turn off USART before reconfiguring it, otherwise incorrect operation may occur */
	UCSR1B = 0;
	UCSR1A = 0;
	UCSR1C = 0;

	/* Set the new baud rate before configuring the USART */
	UBRR1  = SERIAL_2X_UBBRVAL(CDCInterfaceInfo->State.LineEncoding.BaudRateBPS);

	/* Reconfigure the USART in double speed mode for a wider baud rate range at the expense of accuracy */
	UCSR1C = ConfigMask;
	UCSR1A = (1 << U2X1);
	UCSR1B = ((1 << RXCIE1) | (1 << TXEN1) | (1 << RXEN1));
}

