/*
 * lfs_sd.h
 *
 *  Created on: Sep 4, 2024
 *      Author: Sicris Rey Embay
 */

#ifndef FILESYSTEM_LFS_SD_H_
#define FILESYSTEM_LFS_SD_H_

#include "logger_conf.h"

#if CONFIG_USE_LFS_SD

#include "lfs.h"

int32_t lfs_sd_format();
lfs_t * lfs_sd_mount();
int32_t lfs_sd_umount();
int32_t lfs_sd_df();
int32_t lfs_sd_capacity();
int32_t lfs_sd_mkdir(const char * path);
int32_t lfs_sd_ls(const char * path, char * outBuffer, size_t bufferLen);

lfs_file_t * lfs_sd_fopen(const char * pathName);
int32_t lfs_sd_fwrite(const char * strData, size_t len);
int32_t lfs_sd_fread(char * outBuffer, size_t bufLen);
int32_t lfs_sd_mv(const char * source, const char * target);
int32_t lfs_sd_rm(const char * path);
int32_t lfs_sd_fclose();

#endif /* CONFIG_USE_LFS_SD */
#endif /* FILESYSTEM_LFS_SD_H_ */
