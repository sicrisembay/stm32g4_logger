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
    uint32_t file_open_counter;
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


#endif /* CONFIG_USE_FATFS_SD */

