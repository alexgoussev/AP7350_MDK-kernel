/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "bootloader.h"
#include "common.h"
#include "mtdutils/mtdutils.h"
#include "roots.h"

#include "cutils/log.h"
#undef LOG_TAG
#define LOG_TAG "fota"

#if 0
#include "RB_ImageUpdate.h"
#include "RB_vRM_ImageUpdate.h"
#include "RbErrors.h"
#endif

#include "RB_ImageUpdate.h"
#include "RB_vRM_Errors.h"
#include "RB_vRM_Update.h"
#include "RB_FileSystemUpdate.h"

//#include "vRM_PublicDefines.h"
//#include "RbErrors.h"

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

///////////////////////////////////////////////////////////////////////////////

void LOG_INFO(const char *msg, ...)
{
    int err = errno;
    va_list args;
    va_start(args, msg);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), msg, args);
    va_end(args);

    //if (err != 0) {
    //    strlcat(buf, ": ", sizeof(buf));
    //    strlcat(buf, strerror(err), sizeof(buf));
    //}

    //fprintf(stdout, "<fota> %s\n", buf);
    fprintf(stdout, "%s\n", buf);
    //clearerr(stdout);
    fflush(stdout);

    //LOGI("%s\n", buf);
}

void LOG_ERROR(const char *msg, ...)
{
    int err = errno;
    va_list args;
    va_start(args, msg);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), msg, args);
    va_end(args);

    if (err != 0) {
        strlcat(buf, ": ", sizeof(buf));
        strlcat(buf, strerror(err), sizeof(buf));
    }

    //fprintf(stderr, "<fota err> %s\n", buf);
    fprintf(stderr, "<err> %s\n", buf);
    //clearerr(stderr);
    fflush(stderr);

    //LOGE("%s\n", buf);
    //exit(1);
}

void LOG_HEX(const char *str, const char *p, int len)
{
    int i;

    if (str)
        //fprintf(stdout, "<fota> %s", str);
        fprintf(stdout, " %s", str);
    else
        fprintf(stdout, " ");
        //fprintf(stdout, "<fota> ");

    for (i = 0; i < len; i++)  {
        fprintf(stdout, "%02X ", p[i]);
    }
    fprintf(stdout, "\n");

    fflush(stdout);
}


void convert_unicode_to_char(
	const char *src,
	char *dest)
{
    int i;

    for(i=0; src[i] != '\0';i++) {
        dest[i] = (char)src[i];
        if (dest[i] == '\\')
            dest[i] = '/';
    }
    dest[i] = '\0';

    LOG_INFO("[%s] %s", __FUNCTION__, dest);
}

void convert_char_to_unicode(
	const char *src,
	unsigned short *dest)
{
    int i;

    LOG_INFO("[%s] %s", __FUNCTION__, src);

    for (i=0; src[i]; i++) {
        dest[i] = src[i];
        dest[i] &= 0xff;
    }
    dest[i] = '\0';
}

struct FOTAErrorData  {
    int   code;
    char  str[128];
};

static struct FOTAErrorData fota_error_data[] = {
//    { E_RB_BAD_FS_OPERATION,                  "bad operation number for FS update" },
    { E_RB_INVALID_OPERATION,                 "bad operation number update" },
    { E_RB_UNSUPPORTED_COMPRESSION,           "unsupported compression" },
    { E_RB_NO_REVERSE_IN_DELTA,               "Can not apply reverse update for delta not generated as reverse delta" },
    { E_RB_NUM_BCK_LESS_THAN_IN_DELTA,        "number of backup buffers given to UPI does not match number in delta file" },
    { E_RB_SECTOR_SIZE_MISMATCH,              "Sector size mismatch between UPI and delta" },
    { E_RB_UPI_NOT_SUPPORT_REVERSE_UPDATE,    "UPI was not compiled to support reverse update" },
    { E_RB_UPI_NOT_SUPPORT_IFS_ON_COMPRESSED, "UPI was not compiled to support IFS on compressed images" },
//    { E_RB_UPI_FOR_BGU_MUST_SUPPORT_IFS,      "UPI was not compiled to support IFS" },
    { E_RB_SOURCE_FILE_SIG_MISMATCH,          "File signature does not match signature" },
    { E_RB_IN_SCOUT_ONLY_VERIFY,              "In scout only operation we should do only verify of image"},
    { E_RB_NOT_ENOUGH_RAM_FOR_OPERATION2,     "There is not enough RAM to run with operation=2" },
    { E_RB_DELTA_FILE_TOO_LONG,               "Delta file too long - curropted" },
    { E_RB_ERROR_IN_DELETES_SIG,              "Mismatch between deletes sig and delta deletes buffers signature" },
    { E_RB_PKG_NUM_OF_FRAGMENTS_MISMATCH,     "Number of fragments in section is not 1" },
    { E_RB_OVERALL_NUM_BCK_SECTS_TOO_BIG,     "Over all number of backup sects too big" },
    { E_RB_DELTA_IS_CORRUPT,                  "Delta file is corrupt: signature mismatch between delta header signature and calculated signature" },
//    { E_RB_FILE_SIZE_MISMATCH,                "Source file size mismatch from file on device to delta file size" },
    { E_RB_SOURCE_FILE_SIG_MISMATCH,          "File signature does not match signature" },
    { E_RB_TARGET_SIG_MISMATCH,               "Signature for the target buffer does not match the one stored in the delta file"},
    { E_RB_INVALID_BACKUP,                    "Too many dirty buffers" },
    { E_RB_UPI_VERSION_MISMATCH,              "UPI version mismatch between UPI and delta" },
    { E_RB_SCOUT_VERION_MISMATCH,             "Scout version mismatch between UPI and delta" }
};


char *get_error_string(int err_code)
{
    unsigned int  i;

    for (i = 0; i < sizeof(fota_error_data) / sizeof(struct FOTAErrorData); i++)  {
        if (err_code == fota_error_data[i].code)  {
            return fota_error_data[i].str;
        }
    }

    return "Unknown reason";
}

#if 0

void print_error_string(int err_code)
{
    switch (err_code)  {
        case E_RB_BAD_FS_OPERATION:
            LOG_INFO("bad operation number for FS update");
            break;
        case E_RB_INVALID_FW_OPERATION:
            LOG_INFO("bad operation number for FW update");
            break;
        case E_RB_UNSUPPORTED_COMPRESSION:
            LOG_INFO("unsupported compression");
            break;
        case E_RB_NO_REVERSE_IN_DELTA:
            LOG_INFO("Can not apply reverse update for delta not generated as reverse delta");
            break;
        case E_RB_NUM_BCK_LESS_THAN_IN_DELTA:
            LOG_INFO("number of backup buffers given to UPI does not match number in delta file");
            break;
        case E_RB_SECTOR_SIZE_MISMATCH:
            LOG_INFO("Sector size mismatch between UPI and delta");
            break;
        case E_RB_UPI_NOT_SUPPORT_REVERSE_UPDATE:
            LOG_INFO("UPI was not compiled to support reverse update");
            break;
        case E_RB_UPI_NOT_SUPPORT_IFS_ON_COMPRESSED:
            LOG_INFO("UPI was not compiled to support IFS on compressed images");
            break;
        case E_RB_UPI_FOR_BGU_MUST_SUPPORT_IFS:
            LOG_INFO("UPI was not compiled to support IFS");
            break;
        case E_RB_IMAGE_IS_NOT_SOURCE:
            LOG_INFO("Image verified is not source image");
            break;
        case E_RB_IN_SCOUT_ONLY_VERIFY:
            LOG_INFO("In scout only operation we should do only verify of image");
            break;
        case E_RB_NOT_ENOUGH_RAM_FOR_OPERATION2:
            LOG_INFO("There is not enough RAM to run with operation=2");
            break;
        case E_RB_DELTA_FILE_TOO_LONG:
            LOG_INFO("Delta file too long - curropted");
            break;
        case E_RB_ERROR_IN_DELETES_SIG:
            LOG_INFO("Mismatch between deletes sig and delta deletes buffers signature");
            break;
        case E_RB_PKG_NUM_OF_FRAGMENTS_MISMATCH:
            LOG_INFO("Number of fragments in section is not 1");
            break;
        case E_RB_OVERALL_NUM_BCK_SECTS_TOO_BIG:
            LOG_INFO("Over all number of backup sects too big");
            break;
        case E_RB_DELTA_IS_CORRUPT:
            LOG_INFO("Delta file is corrupt: signature mismatch between delta header signature and calculated signature");
            break;
        case E_RB_FILE_SIZE_MISMATCH:
            LOG_INFO("Source file size mismatch from file on device to delta file size");
            break;
        case E_RB_SOURCE_FILE_SIG_MISMATCH:
            LOG_INFO("File signature does not match signature");
            break;
        case E_RB_TARGET_SIG_MISMATCH:
            LOG_INFO("Signature for the target buffer does not match the one stored in the delta file");
            break;
        case E_RB_INVALID_BACKUP:
            LOG_INFO("Too many dirty buffers");
            break;
        case E_RB_UPI_VERSION_MISMATCH:
            LOG_INFO("UPI version mismatch between UPI and delta");
            break;
        case E_RB_SCOUT_VERION_MISMATCH:
            LOG_INFO("Scout version mismatch between UPI and delta");
            break;
    }
}

#endif

#ifdef __cplusplus
}
#endif	/* __cplusplus */
