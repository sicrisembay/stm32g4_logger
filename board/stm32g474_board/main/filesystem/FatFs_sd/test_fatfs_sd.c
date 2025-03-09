/*
 * test_fatfs_sd.c
 *
 *  Created on: Feb 22, 2025
 *      Author: H255182
 */

#include "logger_conf.h"

#if CONFIG_TEST_FATFS_SD

#include "errno.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "stdbool.h"
#include "limits.h"
#include "FreeRTOS.h"
#include "FreeRTOS-Plus-CLI/FreeRTOS_CLI.h"
#include "../../components/filesystem/FatFs/source/ff.h"
#include "fatfs_sd.h"


static bool bInit = false;

static char * const STR_FRESULT[20] = {
        "FR_OK = 0",              /* (0) Function succeeded */
        "FR_DISK_ERR",            /* (1) A hard error occurred in the low level disk I/O layer */
        "FR_INT_ERR",             /* (2) Assertion failed */
        "FR_NOT_READY",           /* (3) The physical drive does not work */
        "FR_NO_FILE",             /* (4) Could not find the file */
        "FR_NO_PATH",             /* (5) Could not find the path */
        "FR_INVALID_NAME",        /* (6) The path name format is invalid */
        "FR_DENIED",              /* (7) Access denied due to a prohibited access or directory full */
        "FR_EXIST",               /* (8) Access denied due to a prohibited access */
        "FR_INVALID_OBJECT",      /* (9) The file/directory object is invalid */
        "FR_WRITE_PROTECTED",     /* (10) The physical drive is write protected */
        "FR_INVALID_DRIVE",       /* (11) The logical drive number is invalid */
        "FR_NOT_ENABLED",         /* (12) The volume has no work area */
        "FR_NO_FILESYSTEM",       /* (13) Could not find a valid FAT volume */
        "FR_MKFS_ABORTED",        /* (14) The f_mkfs function aborted due to some problem */
        "FR_TIMEOUT",             /* (15) Could not take control of the volume within defined period */
        "FR_LOCKED",              /* (16) The operation is rejected according to the file sharing policy */
        "FR_NOT_ENOUGH_CORE",     /* (17) LFN working buffer could not be allocated or given buffer is insufficient in size */
        "FR_TOO_MANY_OPEN_FILES", /* (18) Number of open files > FF_FS_LOCK */
        "FR_INVALID_PARAMETER"    /* (19) Given parameter is invalid */
};

#if CONFIG_FATFS_CMD_FORMAT
static BaseType_t FuncFatFsFormat(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    FRESULT result = FR_OK;
    memset(pcWriteBuffer, 0, xWriteBufferLen);

//    result = fatfs_sd_format();
    if(result == FR_OK) {
        snprintf(pcWriteBuffer, xWriteBufferLen, "\tOK\r\n\r\n");
    } else {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tError: fatfs_sd_mount %d\r\n\r\n", result);
    }
    return 0;
}


static const CLI_Command_Definition_t fatfs_cmd_format = {
    "fatfs_format",
    "fatfs_format:\r\n"
    "\tFormats the storage device\r\n\r\n",
    FuncFatFsFormat,
    0
};
#endif /* CONFIG_FATFS_CMD_FORMAT */

#if CONFIG_FATFS_CMD_MOUNT
static BaseType_t FuncFatFsCmdMount(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    FRESULT res;
    memset(pcWriteBuffer, 0, xWriteBufferLen);

    res = fatfs_sd_mount();
    if(res == FR_OK) {
        snprintf(pcWriteBuffer, xWriteBufferLen, "\tOK\r\n\r\n");
    } else {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tError: %s\r\n\r\n", STR_FRESULT[res]);
    }
    return 0;
}


static const CLI_Command_Definition_t fatfs_cmd_mount = {
    "fatfs_mount",
    "fatfs_mount:\r\n"
    "\tMounts the storage device\r\n\r\n",
    FuncFatFsCmdMount,
    0
};
#endif /* CONFIG_FATFS_CMD_MOUNT */

#if CONFIG_FATFS_CMD_INFO
char * strFatFsType(BYTE type)
{
    char * ret;
    switch(type) {
        case FS_FAT12: {
            ret = "FAT12";
            break;
        }
        case FS_FAT16: {
            ret = "FAT16";
            break;
        }
        case FS_FAT32: {
            ret = "FAT32";
            break;
        }
        case FS_EXFAT: {
            ret = "EXFAT";
            break;
        }
        default: {
            ret = "Invalid FAT";
            break;
        }
    }
    return ret;
}

static BaseType_t FuncFatFsCmdInfo(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    memset(pcWriteBuffer, 0, xWriteBufferLen);

    FATFS const * const instance = fatfs_get_instance();
    if(NULL == instance) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tError: Invalid FAT file system\r\n\r\n");
        return 0;
    }

    snprintf(pcWriteBuffer, xWriteBufferLen,
            "\tType: %s\r\n"
            "\tFAT entry: %ld\r\n"
            "\tSector/FAT: %ld\r\n"
            "\tCluster size: %d\r\n"
            "\tBase Sectors\r\n"
            "\t  Volume: 0x%08lx\r\n"
            "\t  FAT   : 0x%08lx\r\n"
            "\t  RDIR  : 0x%08lx\r\n"
            "\t  DATA  : 0x%08lx\r\n"
            "\tVolume\r\n"
            "\t  label: %s\r\n"
            "\t  SN: %ld\r\n"
            "\r\n",
            strFatFsType(instance->fs_type),
            instance->n_fatent,
            instance->fsize,
            instance->csize,
            instance->volbase,
            instance->fatbase,
            instance->dirbase,
            instance->database,
            fatfs_get_volume_label(),
            fatfs_get_volume_serial_number()
            );

    return 0;
}

static const CLI_Command_Definition_t fatfs_cmd_info = {
    "fatfs_info",
    "fatfs_info:\r\n"
    "\tPrints FAT filesytem information\r\n\r\n",
    FuncFatFsCmdInfo,
    0
};
#endif

#if CONFIG_FATFS_CMD_LS
static BaseType_t FuncFatFsCmdLs(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString )
{
    char * ptrStrParam;
    BaseType_t strParamLen;
    FRESULT res;
    static uint32_t step = 0;
    static DIR dir;

    memset(pcWriteBuffer, 0, xWriteBufferLen);

    if(step == 0) {
        /* Get directory name */
        ptrStrParam = (char *) FreeRTOS_CLIGetParameter(pcCommandString, 1, &strParamLen);
        /* Open directory */
        if(NULL == ptrStrParam) {
            res = f_opendir(&dir, "/");
        } else {
            res = f_opendir(&dir, ptrStrParam);
        }

        if(FR_OK != res) {
            snprintf(pcWriteBuffer, xWriteBufferLen, "\tError: %s\r\n\r\n", STR_FRESULT[res]);
            return 0;
        }
        step++;
        return 1;
    } else if(step == 1) {
        FILINFO fno;
        /* Read directory */
        res = f_readdir(&dir, &fno);
        if((FR_OK != res) || (fno.fname[0] == 0)) {
            step++;
            return 1;
        }
        if(fno.fattrib & AM_DIR) {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                 "\t--d            %s\r\n",
                 fno.fname);
        } else {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                    "\t--- %10ld %s\r\n",
                    (uint32_t)fno.fsize,
                    fno.fname);
        }
        return 1;
    } else {
        step = 0;
        res = f_closedir(&dir);
        if(FR_OK == res) {
            snprintf(pcWriteBuffer, xWriteBufferLen, "\r\n");
        } else {
            snprintf(pcWriteBuffer, xWriteBufferLen, "\tError: %s \r\n\r\n", STR_FRESULT[res]);
        }
        return 0;
    }
}


static const CLI_Command_Definition_t fatfs_cmd_ls = {
    "fatfs_ls",
    "fatfs_ls <directory>:\r\n"
    "\tList the contents of directory\r\n\r\n",
    FuncFatFsCmdLs,
    -1
};
#endif /* CONFIG_FATFS_CMD_LS */

#if CONFIG_FATFS_CMD_FOPEN
static FIL file;

static BaseType_t FuncFatFsCmdFopen(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString)
{
    char * ptrStrParam;
    BaseType_t strParamLen;
    FRESULT res;

    memset(pcWriteBuffer, 0, xWriteBufferLen);

    /* Get file path */
    ptrStrParam = (char *) FreeRTOS_CLIGetParameter(pcCommandString, 1, &strParamLen);
    res = f_open(&file, ptrStrParam, FA_OPEN_ALWAYS | FA_WRITE | FA_READ);
    if(res == FR_OK) {
        snprintf(pcWriteBuffer, xWriteBufferLen, "\tOK\r\n\r\n");
    } else {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tError: %s\r\n\r\n", STR_FRESULT[res]);
    }
    return 0;
}

static const CLI_Command_Definition_t fatfs_cmd_fopen = {
    "fatfs_fopen",
    "fatfs_fopen <file>:\r\n"
    "\tOpens the <file> in append/read/write mode\r\n\r\n",
    FuncFatFsCmdFopen,
    -1
};
#endif /* CONFIG_FATFS_CMD_FOPEN */

#if CONFIG_FATFS_CMD_FCLOSE
static BaseType_t FuncFatFsCmdFclose(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString)
{
    FRESULT res;

    memset(pcWriteBuffer, 0, xWriteBufferLen);

    /* Get file path */
    res = f_close(&file);
    if(res == FR_OK) {
        snprintf(pcWriteBuffer, xWriteBufferLen, "\tOK\r\n\r\n");
    } else {
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\tError: %s\r\n\r\n", STR_FRESULT[res]);
    }
    return 0;
}

static const CLI_Command_Definition_t fatfs_cmd_fclose = {
    "fatfs_fclose",
    "fatfs_fclose:\r\n"
    "\tCloses the file\r\n\r\n",
    FuncFatFsCmdFclose,
    0
};
#endif /* CONFIG_FATFS_CMD_FCLOSE */

#if CONFIG_FATFS_CMD_FREAD
static BaseType_t FuncFatFsCmdFread(
                char *pcWriteBuffer,
                size_t xWriteBufferLen,
                const char *pcCommandString)
{
    FRESULT res;
    uint8_t buff[32];
    static uint32_t step = 0;
    UINT br;

    memset(pcWriteBuffer, 0, xWriteBufferLen);

    if(step == 0) {
        res = f_rewind(&file);
        if(res == FR_OK) {
            step++;
            return 1;
        } else {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                    "\tError: %s\r\n\r\n", STR_FRESULT[res]);
            return 0;
        }
    } else if(step == 1) {
        res = f_read(&file, (void*)buff, sizeof(buff), &br);
        if(res != FR_OK) {
            step++;
            snprintf(pcWriteBuffer, xWriteBufferLen,
                    "\tError: %s\r\n\r\n", STR_FRESULT[res]);
            return 1;
        } else {
            if(br > 0) {
                memcpy(pcWriteBuffer, buff, br);
            }
            if(br < sizeof(buff)) {
                step++;
            }
            return 1;
        }
    } else {
        step = 0;
        snprintf(pcWriteBuffer, xWriteBufferLen,
                "\r\n");
    }

    return 0;
}

static const CLI_Command_Definition_t fatfs_cmd_fread = {
    "fatfs_fread",
    "fatfs_fread:\r\n"
    "\tPrints the contents of an opened file\r\n\r\n",
    FuncFatFsCmdFread,
    0
};
#endif /* CONFIG_FATFS_CMD_FREAD */

void TEST_FatFS_Init(void)
{
    if(bInit) {
        return;
    }

#if CONFIG_FATFS_CMD_FORMAT
    FreeRTOS_CLIRegisterCommand(&fatfs_cmd_format);
#endif

#if CONFIG_FATFS_CMD_MOUNT
    FreeRTOS_CLIRegisterCommand(&fatfs_cmd_mount);
#endif

#if CONFIG_FATFS_CMD_INFO
    FreeRTOS_CLIRegisterCommand(&fatfs_cmd_info);
#endif

#if CONFIG_FATFS_CMD_LS
    FreeRTOS_CLIRegisterCommand(&fatfs_cmd_ls);
#endif

#if CONFIG_FATFS_CMD_FOPEN
    FreeRTOS_CLIRegisterCommand(&fatfs_cmd_fopen);
#endif

#if CONFIG_FATFS_CMD_FREAD
    FreeRTOS_CLIRegisterCommand(&fatfs_cmd_fclose);
#endif

#if CONFIG_FATFS_CMD_FREAD
    FreeRTOS_CLIRegisterCommand(&fatfs_cmd_fread);
#endif


    bInit = true;
}

#else
void TEST_FatFS_Init(void)
{

}
#endif /* CONFIG_TEST_FATFS_SD */
