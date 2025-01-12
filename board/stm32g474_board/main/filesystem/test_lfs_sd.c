/*
 * test_lfs_sd.c
 *
 *  Created on: Sep 5, 2024
 *      Author: Sicris Rey Embay
 */

#include "logger_conf.h"

#if CONFIG_TEST_LFS_SD

#include "errno.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "stdbool.h"
#include "limits.h"
#include "FreeRTOS.h"
#include "FreeRTOS-Plus-CLI/FreeRTOS_CLI.h"
#include "lfs.h"
#include "lfs_sd.h"

#define LFS_SD_MAX_PATHNAME_LENGTH      (LFS_NAME_MAX)

static bool bInit = false;

static BaseType_t FuncLfsCmdFormat(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    memset(pcWriteBuffer, 0, xWriteBufferLen);

    int32_t ret = lfs_sd_format();
    if(LFS_ERR_OK != ret) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tError: lfs_sd_format %ld\r\n\r\n", ret);
        return 0;
    }
    snprintf(pcWriteBuffer, xWriteBufferLen, "\tOK\r\n\r\n");
    return 0;
}

static const CLI_Command_Definition_t lfs_cmd_format = {
    "lfs_format",
    "lfs_format:\r\n"
    "\tFormats the storage device\r\n\r\n",
    FuncLfsCmdFormat,
    0
};


static BaseType_t FuncLfsCmdStat(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    memset(pcWriteBuffer, 0, xWriteBufferLen);

    const struct lfs_config * pConfig = lfs_sd_stat();
    snprintf(pcWriteBuffer, xWriteBufferLen,
            "\tread_size: %ld\r\n"
            "\tprog_size: %ld\r\n"
            "\tblock_size: %ld\r\n"
            "\tblock_count: %ld\r\n"
            "\tblock_cycles: %ld\r\n"
            "\tcache_size: %ld\r\n"
            "\tlookahead_size: %ld\r\n"
            "\tname_max: %ld\r\n"
            "\tfile_max: %ld\r\n"
            "\tattr_max: %ld\r\b"
            "\tmetadata_max: %ld\r\n"
            "\tinline_max: %ld\r\n"
            "\r\n",
            pConfig->read_size, pConfig->prog_size,
            pConfig->block_size, pConfig->block_count,
            pConfig->block_cycles, pConfig->cache_size,
            pConfig->lookahead_size, pConfig->name_max,
            pConfig->file_max, pConfig->attr_max,
            pConfig->metadata_max, pConfig->inline_max);

    return 0;
}


static const CLI_Command_Definition_t lfs_cmd_stat = {
    "lfs_stat",
    "lfs_stat:\r\n"
    "\tStat of LFS\r\n\r\n",
    FuncLfsCmdStat,
    0
};


static BaseType_t FuncLfsCmdMount(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    memset(pcWriteBuffer, 0, xWriteBufferLen);

    if(lfs_sd_mount()) {
        snprintf(pcWriteBuffer, xWriteBufferLen, "\tOK\r\n\r\n");
    } else {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tError: lfs_sd_mount\r\n\r\n");
    }
    return 0;
}

static const CLI_Command_Definition_t lfs_cmd_mount = {
    "lfs_mount",
    "lfs_mount:\r\n"
    "\tMounts the storage device\r\n\r\n",
    FuncLfsCmdMount,
    0
};


static BaseType_t FuncLfsCmdUmount(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    memset(pcWriteBuffer, 0, xWriteBufferLen);

    if(LFS_ERR_OK == lfs_sd_umount()) {
        snprintf(pcWriteBuffer, xWriteBufferLen, "\tOK\r\n\r\n");
    } else {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tError: lfs_sd_umount\r\n\r\n");
    }
    return 0;
}

static const CLI_Command_Definition_t lfs_cmd_umount = {
    "lfs_umount",
    "lfs_umount:\r\n"
    "\tUnmounts the storage device\r\n\r\n",
    FuncLfsCmdUmount,
    0
};


static BaseType_t FuncLfsCmdDf(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    memset(pcWriteBuffer, 0, xWriteBufferLen);

    const int32_t nAllocBlock = lfs_sd_df();
    const int32_t nTotalBlock = lfs_sd_capacity();
    if((nAllocBlock < 0) || (nTotalBlock <= 0)) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tError: lfs_sd_df\r\n\r\n");
        return 0;
    }
    const int32_t nFreeBlock = nTotalBlock - nAllocBlock;
    const int32_t used = (nAllocBlock * 100) / nTotalBlock;
    snprintf(pcWriteBuffer, xWriteBufferLen,
            "\tFree Blocks: %ld\r\n"
            "\tUsed: %ld/%ld (%ld%%)\r\n\r\n",
            nFreeBlock,
            nAllocBlock,
            nTotalBlock,
            used);
    return 0;
}

static const CLI_Command_Definition_t lfs_cmd_df = {
    "lfs_df",
    "lfs_df:\r\n"
    "\tDisk free space\r\n\r\n",
    FuncLfsCmdDf,
    0
};

static BaseType_t FuncLfsCmdLs(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    char * ptrStrParam;
    BaseType_t strParamLen;

    memset(pcWriteBuffer, 0, xWriteBufferLen);

    /* Get directory name */
    ptrStrParam = (char *) FreeRTOS_CLIGetParameter(pcCommandString, 1, &strParamLen);
    if(NULL == ptrStrParam) {
        if(LFS_ERR_OK != lfs_sd_ls("/", pcWriteBuffer, xWriteBufferLen)) {
            strncat(pcWriteBuffer, "\tFailed\r\n\r\n", xWriteBufferLen);
        }
    } else {
        if(LFS_ERR_OK != lfs_sd_ls(ptrStrParam, pcWriteBuffer, xWriteBufferLen)) {
            strncat(pcWriteBuffer, "\tFailed\r\n\r\n", xWriteBufferLen);
        }
    }
    return 0;
}

static const CLI_Command_Definition_t lfs_cmd_ls = {
    "lfs_ls",
    "lfs_ls <directory>:\r\n"
    "\tList the contents of directory\r\n\r\n",
    FuncLfsCmdLs,
    -1
};


static BaseType_t FuncLfsCmdMkDir(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    char * ptrStrParam;
    BaseType_t strParamLen;

    memset(pcWriteBuffer, 0, xWriteBufferLen);

    /* Get directory name */
    ptrStrParam = (char *) FreeRTOS_CLIGetParameter(pcCommandString, 1, &strParamLen);
    if(NULL == ptrStrParam) {
        snprintf(pcWriteBuffer, xWriteBufferLen, "\tError: Parameter1 not found\r\n\r\n");
        return 0;
    }

    if(LFS_ERR_OK != lfs_sd_mkdir(ptrStrParam)) {
        snprintf(pcWriteBuffer, xWriteBufferLen, "\tError: mkdir\r\n\r\n");
        return 0;
    }

    snprintf(pcWriteBuffer, xWriteBufferLen, "\tOK\r\n\r\n");
    return 0;
}

static const CLI_Command_Definition_t lfs_cmd_mkdir = {
    "lfs_mkdir",
    "lfs_mkdir <directory>:\r\n"
    "\tCreates a directory\r\n\r\n",
    FuncLfsCmdMkDir,
    1
};


static BaseType_t FuncLfsCmdFopen(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    char * ptrStrParam;
    BaseType_t strParamLen;

    memset(pcWriteBuffer, 0, xWriteBufferLen);

    /* Get file name */
    ptrStrParam = (char *)FreeRTOS_CLIGetParameter(pcCommandString, 1, &strParamLen);
    if(NULL == ptrStrParam) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tError: Parameter1 not found!\r\n\r\n");
        return 0;
    }

    if(NULL != lfs_sd_fopen(ptrStrParam)) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tOK\r\n\r\n");
    } else {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tError: open failed!\r\n\r\n");
    }
    return 0;
}

static const CLI_Command_Definition_t lfs_cmd_fopen = {
    "lfs_fopen",
    "lfs_fopen <filename>:\r\n"
    "\tOpen file in RDWR mode\r\n\r\n",
    FuncLfsCmdFopen,
    1
};


static BaseType_t FuncLfsCmdFclose(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    memset(pcWriteBuffer, 0, xWriteBufferLen);

    if(LFS_ERR_OK != lfs_sd_fclose()) {
        snprintf(pcWriteBuffer, xWriteBufferLen, "\tError: fclose\r\n\r\n");
        return 0;
    }
    snprintf(pcWriteBuffer, xWriteBufferLen, "\tOK\r\n\r\n");
    return 0;
}

static const CLI_Command_Definition_t lfs_cmd_fclose = {
    "lfs_fclose",
    "lfs_fclose:\r\n"
    "\tCloses already opened file\r\n\r\n",
    FuncLfsCmdFclose,
    0
};


static BaseType_t FuncLfsCmdFwrite(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    char * ptrStrParam;
    BaseType_t strParamLen;

    memset(pcWriteBuffer, 0, xWriteBufferLen);

    ptrStrParam = (char *) FreeRTOS_CLIGetParameter(pcCommandString, 1, &strParamLen);
    if(NULL == ptrStrParam) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tNothing to write!\r\n\r\n");
        return 0;
    }
    const size_t len = strlen(ptrStrParam);
    if(len == lfs_sd_fwrite(ptrStrParam, len)) {
        snprintf(pcWriteBuffer, xWriteBufferLen, "\tOK\r\n\r\n");
    } else {
        snprintf(pcWriteBuffer, xWriteBufferLen, "\tFailed\r\n\r\n");
    }
    return 0;
}

static const CLI_Command_Definition_t lfs_cmd_fwrite = {
    "lfs_fwrite",
    "lfs_fwrite <data>:\r\n"
    "\tAppends <data> to already opened file\r\n\r\n",
    FuncLfsCmdFwrite,
    -1
};


static BaseType_t FuncLfsCmdFread(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    memset(pcWriteBuffer, 0, xWriteBufferLen);

    int32_t ret = lfs_sd_fread(pcWriteBuffer, xWriteBufferLen - 4);

    if(ret == 0) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tEmpty File\r\n\r\n");
    } else if(ret < 0) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tFailed with error %ld\r\n\r\n", ret);
    } else {
        strncat(pcWriteBuffer, "\r\n\r\n", xWriteBufferLen);
    }
    return 0;
}

static const CLI_Command_Definition_t lfs_cmd_fread = {
    "lfs_fread",
    "lfs_fread:\r\n"
    "\tReads from already opened file\r\n\r\n",
    FuncLfsCmdFread,
    0
};

static BaseType_t FuncLfsCmdMv(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    char * ptrStrParam;
    BaseType_t strParamLen;
    char src[LFS_SD_MAX_PATHNAME_LENGTH];
    char target[LFS_SD_MAX_PATHNAME_LENGTH];

    memset(pcWriteBuffer, 0, xWriteBufferLen);

    /* Get source name */
    ptrStrParam = (char *)FreeRTOS_CLIGetParameter(pcCommandString, 1, &strParamLen);
    if(ptrStrParam == NULL) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tError: Parameter1 not found!\r\n\r\n");
        return 0;
    }
    if(strParamLen > (LFS_SD_MAX_PATHNAME_LENGTH - 1)) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tError: <src> length exceeds %d\r\n\r\n",
                LFS_SD_MAX_PATHNAME_LENGTH);
        return 0;
    }
    strncpy(src, ptrStrParam, strParamLen);
    src[strParamLen] = '\0';

    /* Get target name */
    ptrStrParam = (char *)FreeRTOS_CLIGetParameter(pcCommandString, 2, &strParamLen);
    if(ptrStrParam == NULL) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tError: Parameter2 not found!\r\n\r\n");
        return 0;
    }
    if(strParamLen > (LFS_SD_MAX_PATHNAME_LENGTH - 1)) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tError: <target> length exceeds %d\r\n\r\n",
                LFS_SD_MAX_PATHNAME_LENGTH);
        return 0;
    }
    strncpy(target, ptrStrParam, strParamLen);
    target[strParamLen] = '\0';

    if(LFS_ERR_OK == lfs_sd_mv(src, target)) {
        snprintf(pcWriteBuffer, xWriteBufferLen, "\tOK\r\n\r\n");
    } else {
        snprintf(pcWriteBuffer, xWriteBufferLen, "\tFailed\r\n\r\n");
    }

    return 0;
}

static const CLI_Command_Definition_t lfs_cmd_mv = {
    "lfs_mv",
    "lfs_mv <src> <target>:\r\n"
    "\tMoves or renames <src> to <target>\r\n\r\n",
    FuncLfsCmdMv,
    2
};


static BaseType_t FuncLfsCmdRm(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    char * ptrStrParam;
    BaseType_t strParamLen;
    char path[LFS_SD_MAX_PATHNAME_LENGTH];

    memset(pcWriteBuffer, 0, xWriteBufferLen);

    /* Get path name */
    ptrStrParam = (char *)FreeRTOS_CLIGetParameter(pcCommandString, 1, &strParamLen);
    if(ptrStrParam == NULL) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tError: Parameter1 not found!\r\n\r\n");
        return 0;
    }
    if(strParamLen > (LFS_SD_MAX_PATHNAME_LENGTH - 1)) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tError: <path> length exceeds %ld\r\n\r\n",
                LFS_SD_MAX_PATHNAME_LENGTH);
        return 0;
    }
    strncpy(path, ptrStrParam, strParamLen);
    path[strParamLen] = '\0';

    if(LFS_ERR_OK == lfs_sd_rm(path)) {
        snprintf(pcWriteBuffer, xWriteBufferLen, "\tOK\r\n\r\n");
    } else {
        snprintf(pcWriteBuffer, xWriteBufferLen, "\tFailed\r\n\r\n");
    }

    return 0;
}

static const CLI_Command_Definition_t lfs_cmd_rm = {
    "lfs_rm",
    "lfs_rm <path>:\r\n"
    "\tRemoves file or directory specified by <path>\r\n\r\n",
    FuncLfsCmdRm,
    1
};


void TEST_LFS_Init(void)
{
    if(bInit) {
        return;
    }

    FreeRTOS_CLIRegisterCommand(&lfs_cmd_format);
    FreeRTOS_CLIRegisterCommand(&lfs_cmd_stat);
    FreeRTOS_CLIRegisterCommand(&lfs_cmd_mount);
    FreeRTOS_CLIRegisterCommand(&lfs_cmd_umount);
    FreeRTOS_CLIRegisterCommand(&lfs_cmd_df);
    FreeRTOS_CLIRegisterCommand(&lfs_cmd_ls);
    FreeRTOS_CLIRegisterCommand(&lfs_cmd_mkdir);
    FreeRTOS_CLIRegisterCommand(&lfs_cmd_fopen);
    FreeRTOS_CLIRegisterCommand(&lfs_cmd_fclose);
    FreeRTOS_CLIRegisterCommand(&lfs_cmd_fwrite);
    FreeRTOS_CLIRegisterCommand(&lfs_cmd_fread);
    FreeRTOS_CLIRegisterCommand(&lfs_cmd_mv);
    FreeRTOS_CLIRegisterCommand(&lfs_cmd_rm);

    bInit = true;
}

#endif /* CONFIG_TEST_LFS_SD */
