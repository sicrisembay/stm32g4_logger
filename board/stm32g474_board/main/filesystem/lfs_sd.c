/*
 * lfs_sd.c
 *
 *  Created on: Sep 4, 2024
 *      Author: Sicris Rey Embay
 */

#include "lfs.h"
#include "lfs_sd.h"

int32_t lfs_sd_format()
{
    return LFS_ERR_OK;
}


lfs_t * lfs_sd_mount()
{
    return (lfs_t *)0;
}


int32_t lfs_sd_umount()
{
    return LFS_ERR_OK;
}


int32_t lfs_sd_mkdir(const char * path)
{
    return LFS_ERR_OK;
}


int32_t lfs_sd_ls(const char * path, char * outBuffer, size_t bufferLen)
{
    return LFS_ERR_OK;
}


lfs_file_t * lfs_sd_fopen(const char * pathName)
{
    return (lfs_file_t *)0;
}


int32_t lfs_sd_fwrite(const char * strData, size_t len)
{
    return LFS_ERR_OK;
}


int32_t lfs_sd_fread(char * outBuffer, size_t bufLen)
{
    return LFS_ERR_OK;
}


int32_t lfs_sd_mv(const char * source, const char * target)
{
    return LFS_ERR_OK;
}


int32_t lfs_sd_rm(const char * path)
{
    return LFS_ERR_OK;
}


int32_t lfs_sd_fclose()
{
    return LFS_ERR_OK;
}


uint32_t lfs_crc(uint32_t crc, const void *buffer, size_t size) {
    static const uint32_t rtable[16] = {
        0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
        0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
        0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
        0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c,
    };

    const uint8_t *data = buffer;

    for (size_t i = 0; i < size; i++) {
        crc = (crc >> 4) ^ rtable[(crc ^ (data[i] >> 0)) & 0xf];
        crc = (crc >> 4) ^ rtable[(crc ^ (data[i] >> 4)) & 0xf];
    }

    return crc;
}
