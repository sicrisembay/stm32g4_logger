/*
 * fatfs_sd.c
 *
 *  Created on: Feb 22, 2025
 *      Author: Sicris Rey Embay
 */

#include "logger_conf.h"

#if CONFIG_USE_FATFS_SD

#include "string.h"
#include "stdio.h"
#include "../../components/filesystem/FatFs/source/ffconf.h"
#include "../../components/filesystem/FatFs/source/ff.h"
#include "fatfs_sd.h"

typedef struct {
    FATFS fs;
    FIL file;
    DIR dir;
    FILINFO fno;
    TCHAR volume_label[34];
    DWORD volume_serial_number;
} fatfs_sd_t;

static fatfs_sd_t fatfs_sd;


FATFS * fatfs_get_instance(void)
{
    fatfs_sd_t * const me = &fatfs_sd;
    if((me->fs.fs_type < 1) || (me->fs.fs_type > 4)) {
        /* invalid FAT filesytem */
        return NULL;
    }

    return &(me->fs);
}


TCHAR * fatfs_get_volume_label(void)
{
    fatfs_sd_t * const me = &fatfs_sd;
    if((me->fs.fs_type < 1) || (me->fs.fs_type > 4)) {
        /* invalid FAT filesytem */
        return NULL;
    }
    return me->volume_label;
}

DWORD fatfs_get_volume_serial_number(void)
{
    fatfs_sd_t * const me = &fatfs_sd;
    if((me->fs.fs_type < 1) || (me->fs.fs_type > 4)) {
        /* invalid FAT filesytem */
        return 0;
    }
    return me->volume_serial_number;
}


FRESULT fatfs_sd_mount(void)
{
    FRESULT result;
    fatfs_sd_t * const me = &fatfs_sd;
    result = f_mount(&me->fs, "", 1);
    if(result != FR_OK) {
        return result;
    }
    result = f_getlabel("", me->volume_label, &me->volume_serial_number);
    return result;
}


FRESULT fatfs_sd_ls(const char * path, char *outBuffer, size_t bufferLen)
{
    FRESULT res;
    fatfs_sd_t * const me = &fatfs_sd;
    size_t remaining;

    res = f_opendir(&me->dir, path);
    if(res != FR_OK) {
        return res;
    }

    while (1) {
        res = f_readdir(&me->dir, &me->fno);
        if (res != FR_OK || me->fno.fname[0] == 0) {
            break;  // Break on error or end of dir
        }

        remaining = bufferLen - strlen(outBuffer);
        if(remaining <= 4) {
            break;
        }
        if (me->fno.fattrib & AM_DIR) {
            outBuffer = strncat(outBuffer, "--d ", remaining);
        } else {
            outBuffer = strncat(outBuffer, "--- ", remaining);
        }

        remaining = bufferLen - strlen(outBuffer);
        if(remaining <= strlen(me->fno.fname)) {
            break;
        }
        strncat(outBuffer, me->fno.fname, remaining);

        remaining = bufferLen - strlen(outBuffer);
        if(remaining <= 2) {
            break;
        }
        strncat(outBuffer, "\r\n", remaining);
    }
    f_closedir(&me->dir);  // Close the directory

    return res;
}

#endif /* CONFIG_USE_FATFS_SD */

