/*!
 ******************************************************************************
 * @file           : usb_msc.c
 * @author         : Sicris Rey Embay
 ******************************************************************************
 */

#include "bsp/board_api.h"
#include "tusb.h"
#include "sdcard.h"
#include  "usb_msc.h"

#if CFG_TUD_MSC

static bool isEjected = true;       // Do not mount storage device by default

// Invoked when received SCSI_CMD_INQUIRY
// Application fill vendor id, product id and revision with string up to 8, 16, 4 characters respectively
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4])
{
    (void) lun;

    const char vid[] = "TinyUSB";
    const char pid[] = "Mass Storage";
    const char rev[] = "1.0";

    memcpy(vendor_id  , vid, strlen(vid));
    memcpy(product_id , pid, strlen(pid));
    memcpy(product_rev, rev, strlen(rev));
}


// Invoked when received Test Unit Ready command.
// return true allowing host to read/write this LUN e.g SD card inserted
bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
    if(isEjected) {
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
        return false;
    } else {
        return SDCARD_ready();
    }
}


// Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size
// Application update block count and block size
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size)
{
    (void) lun;

    *block_count = (uint32_t)SDCARD_GetBlockCount();
    *block_size  = SDCARD_BLOCK_SIZE;
}


// Invoked when received Start Stop Unit command
// - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
// - Start = 1 : active mode, if load_eject = 1 : load disk storage
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject)
{
    (void) lun;
    (void) power_condition;

    if (load_eject) {
        if (start) {
            // load disk storage
        } else {
            // unload disk storage
            isEjected = true;
        }
    }

    return true;
}


// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize)
{
    (void) lun;

    if((bufsize != SDCARD_BLOCK_SIZE) || (offset != 0)) {
        return -1;
    }
    SDCARD_ReadSingleBlock(lba, (uint8_t *)buffer, bufsize);

    return bufsize;
}


// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize)
{
    (void) lun;

    if((bufsize != SDCARD_BLOCK_SIZE) || (offset != 0)) {
        return -1;
    }
    SDCARD_WriteSingleBlock(lba, buffer, bufsize);

    return bufsize;
}


// Callback invoked when received an SCSI command not in built-in list below
// - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, MODE_SENSE6, REQUEST_SENSE
// - READ10 and WRITE10 has their own callbacks
int32_t tud_msc_scsi_cb (uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize)
{
    // read10 & write10 has their own callback and MUST not be handled here

    void const* response = NULL;
    int32_t resplen = 0;

    // most scsi handled is input
    bool in_xfer = true;

    switch (scsi_cmd[0]) {
        default: {
            // Set Sense = Invalid Command Operation
            tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

            // negative means error -> tinyusb could stall and/or response with failed status
            resplen = -1;
            break;
        }
    }

    // return resplen must not larger than bufsize
    if ( resplen > bufsize ) {
    	resplen = bufsize;
    }

    if ( response && (resplen > 0) ) {
        if(in_xfer) {
            memcpy(buffer, response, resplen);
        } else {
          // SCSI output
        }
    }

    return resplen;
}

void usb_msc_mount(void)
{
    isEjected = false;
}

void usb_msc_unmount(void)
{
    isEjected = true;
}

#endif /* CFG_TUD_MSC */

