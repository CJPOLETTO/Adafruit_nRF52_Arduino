/**************************************************************************/
/*!
    @file     usb_msc_flash.c
    @author   hathach (tinyusb.org)

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2018, Adafruit Industries (adafruit.com)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/**************************************************************************/
#if NRF52840_XXAA

#include "tusb.h"

#if CFG_TUD_MSC

#include "nrf_gpio.h"
#include "flash/flash_qspi.h"

// Callback invoked when received an SCSI command not in built-in list below
// - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, MODE_SENSE6, REQUEST_SENSE
// - READ10 and WRITE10 has their own callbacks
int32_t tud_msc_scsi_cb (uint8_t lun, const uint8_t scsi_cmd[16], void* buffer, uint16_t bufsize)
{
  const void* response = NULL;
  uint16_t resplen = 0;

  switch ( scsi_cmd[0] )
  {
    case SCSI_CMD_TEST_UNIT_READY:
      // Command that host uses to check our readiness before sending other commands
      resplen = 0;
    break;

    case SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL:
      // Host is about to read/write etc ... better not to disconnect disk
      resplen = 0;
    break;

    case SCSI_CMD_START_STOP_UNIT:
      // Host try to eject/safe remove/poweroff us. We could safely disconnect with disk storage, or go into lower power
      /* scsi_start_stop_unit_t const * start_stop = (scsi_start_stop_unit_t const *) scsi_cmd;
       // Start bit = 0 : low power mode, if load_eject = 1 : unmount disk storage as well
       // Start bit = 1 : Ready mode, if load_eject = 1 : mount disk storage
       start_stop->start;
       start_stop->load_eject;
       */
      resplen = 0;
    break;

    default:
      // Set Sense = Invalid Command Operation
      tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

      // negative means error -> tinyusb could stall and/or response with failed status
      resplen = -1;
    break;
  }

  // return len must not larger than bufsize
  if ( resplen > bufsize )
  {
    resplen = bufsize;
  }

  // copy response to stack's buffer if any
  if ( response && resplen )
  {
    memcpy(buffer, response, resplen);
  }

  return resplen;
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
int32_t tud_msc_read10_cb (uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize)
{
  (void) lun;

  return flash_qspi_read(buffer, lba * CFG_TUD_MSC_BLOCK_SZ + offset, bufsize);
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
int32_t tud_msc_write10_cb (uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize)
{
  (void) lun;

  uint32_t wrcount = flash_qspi_write(lba * CFG_TUD_MSC_BLOCK_SZ + offset, buffer, bufsize);

  // update fatfs's cache if address matches
  extern ExternalFS_usbmsc_write (uint32_t lba, void const* buffer, uint32_t bufsize) ATTR_WEAK;
  if ( ExternalFS_usbmsc_write ) ExternalFS_usbmsc_write(lba, buffer, bufsize);

  return wrcount;
}

// Callback invoked when WRITE10 command is completed (status received and accepted by host).
// used to flush any pending cache.
void tud_msc_write10_complete_cb (uint8_t lun)
{
  (void) lun;

  // flush pending cache when write10 is complete
  flash_qspi_flush();
}

#endif // CFG_TUD_MSC
#endif // nrf52840
