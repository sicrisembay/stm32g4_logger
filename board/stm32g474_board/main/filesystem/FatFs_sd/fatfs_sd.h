/*
 * fatfs_sd.h
 *
 *  Created on: Feb 22, 2025
 *      Author: Sicris Rey Embay
 */

#ifndef FATFS_SD_H_
#define FATFS_SD_H_

#include "logger_conf.h"

#if CONFIG_USE_FATFS_SD

#include "stddef.h"
#include "../../components/filesystem/FatFs/source/ff.h"

//FRESULT fatfs_sd_format(void);

FATFS * fatfs_get_instance(void);
TCHAR * fatfs_get_volume_label(void);
DWORD fatfs_get_volume_serial_number(void);
FRESULT fatfs_sd_mount(void);
FRESULT fatfs_sd_ls(const char * path, char *outBuffer, size_t bufferLen);

#endif /* CONFIG_USE_FATFS_SD */
#endif /* FATFS_SD_H_ */
