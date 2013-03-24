/** \file
 *
 *      (c) 2013  Robin Scheibler, Safecast (fakufaku [at] gmail [dot] com)
 *  Header file for SDCardManager.c.
 */

#ifndef _SD_MANAGER_H
#define _SD_MANAGER_H

/* Includes: */
#include <avr/io.h>

#include "USBtoSerialMassStorage.h"
#include "Descriptors.h"

#include <LUFA/Common/Common.h>
#include <LUFA/Drivers/USB/USB.h>
//#include <LUFA/Drivers/Board/Dataflash.h>

/* Defines: */
/** Block size of the device. This is kept at 512 to remain compatible with the OS despite the underlying
 *  storage media (Dataflash) using a different native block size. Do not change this value.
 */
#define VIRTUAL_MEMORY_BLOCK_SIZE           512

#define LUN_MEDIA_BLOCKS           (SDCardManager_GetNbBlocks() / TOTAL_LUNS)

/* Function Prototypes: */
void SDCardManager_Init(void);
uint32_t SDCardManager_GetNbBlocks(void);
void SDCardManager_WriteBlocks(USB_ClassInfo_MS_Device_t* const MSInterfaceInfo, const uint32_t BlockAddress, uint16_t TotalBlocks);
void SDCardManager_ReadBlocks(USB_ClassInfo_MS_Device_t* const MSInterfaceInfo, uint32_t BlockAddress, uint16_t TotalBlocks);
void SDCardManager_WriteBlocks_RAM(const uint32_t BlockAddress, uint16_t TotalBlocks,
    uint8_t* BufferPtr) ATTR_NON_NULL_PTR_ARG(3);
void SDCardManagerManager_ReadBlocks_RAM(const uint32_t BlockAddress, uint16_t TotalBlocks,
    uint8_t* BufferPtr) ATTR_NON_NULL_PTR_ARG(3);
void SDCardManager_ResetDataflashProtections(void);
bool SDCardManager_CheckDataflashOperation(void);

#endif
