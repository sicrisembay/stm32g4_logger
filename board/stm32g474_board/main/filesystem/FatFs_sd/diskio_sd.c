/*
 * diskio_sd.c
 *
 *  Created on: Feb 22, 2025
 *      Author: Sicris Rey Embay
 */
#include "logger_conf.h"

#if CONFIG_USE_FATFS_SD

#include "../../components/filesystem/FatFs/source/ff.h"
#include "../../components/filesystem/FatFs/source/diskio.h"
#include "sdcard.h"

static volatile DSTATUS Stat = STA_NOINIT;

DSTATUS disk_status (BYTE pdrv)
{
    (void)pdrv;  // unused
    Stat |= STA_NOINIT;

    if(SDCARD_ready()) {
        Stat &= ~STA_NOINIT;
    }

    return Stat;
}


DSTATUS disk_initialize (BYTE pdrv)
{
    return disk_status(pdrv);
}


DRESULT disk_read (BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    int32_t ret;
    uint8_t *pByte;
    uint32_t block_addr;
    uint32_t block_nbr = count;

    if(buff == NULL) {
        return RES_PARERR;
    }
    pByte = (uint8_t *)buff;
    block_addr = sector;

    do {
        ret = SDCARD_ReadSingleBlock(block_addr, pByte, SDCARD_BLOCK_SIZE);
        if(ret != SDCARD_ERR_NONE) {
            return RES_ERROR;
        }
        pByte += SDCARD_BLOCK_SIZE;
        block_addr++;
        block_nbr--;
    } while (block_nbr > 0U);

    return RES_OK;
}



#if FF_FS_READONLY == 0

DRESULT disk_write (BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    int32_t ret;
    uint8_t *pByte;
    uint32_t block_addr;
    uint32_t block_nbr = count;

    if(buff == NULL) {
        return RES_PARERR;
    }

    pByte = (uint8_t *)buff;
    block_addr = sector;
    do {
        ret = SDCARD_WriteSingleBlock(block_addr, pByte, SDCARD_BLOCK_SIZE);
        if(ret != SDCARD_ERR_NONE) {
            return RES_ERROR;
        }
        pByte += SDCARD_BLOCK_SIZE;
        block_addr++;
        block_nbr--;
    } while(block_nbr > 0U);

    return RES_OK;
}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void *buff)
{
    DRESULT res = RES_OK;

    if((Stat & STA_NOINIT) == STA_NOINIT) {
        return RES_NOTRDY;
    }

    switch(cmd) {
        case CTRL_SYNC: {
            res = RES_OK;
            break;
        }
        case GET_SECTOR_COUNT: {
            *((DWORD*)buff) = SDCARD_GetBlockCount();
            res = RES_OK;
            break;
        }
        case GET_SECTOR_SIZE: {
            *((WORD *)buff) = SDCARD_BLOCK_SIZE;
            res = RES_OK;
            break;
        }
        case GET_BLOCK_SIZE: {
            *((DWORD *)buff) = (SDCARD_SECTOR_SIZE / SDCARD_BLOCK_SIZE);
            res = RES_OK;
            break;
        }
        default: {
            res = RES_PARERR;
            break;
        }
    }
    return res;
}

#endif /* CONFIG_USE_FATFS_SD */

