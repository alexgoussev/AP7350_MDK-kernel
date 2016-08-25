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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdbool.h>

#include "bootloader.h"
#include "common.h"
#include "mtdutils/mtdutils.h"
#include "cutils/android_reboot.h"
#include "roots.h"
#include "install.h"
#include "ui.h"

#include "cutils/log.h"
#undef LOG_TAG
#define LOG_TAG "fota"

#include "fota.h"

#if 0
#include "vRM_PublicDefines.h"
#include "RB_ImageUpdate.h"
#include "RB_vRM_ImageUpdate.h"
#include "RbErrors.h"
#endif

#include "RB_ImageUpdate.h"
#include "RB_vRM_Errors.h"
#include "RB_vRM_Update.h"
#include "RB_FileSystemUpdate.h"


#ifdef SUPPORT_SBOOT_UPDATE
#include "sec/sec.h"
#endif


#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

// ----------------------------------------------------------------------------
//
// Global Config
//

#define BOOT_DELTA_FILE     "boot.delta"
#define SYSTEM_DELTA_FILE   "system.delta"
#define RECOVERY_DELTA_FILE "recovery.delta"
#define TEE1_DELTA_FILE 	"tee1.delta"
#define TEE2_DELTA_FILE 	"tee2.delta"
#define CUSTOM_DELTA_FILE   "custom.delta"

#define BOOT_IMAGE_FILE     "boot.img"
#define SYSTEM_IMAGE_FILE   "system.img"
#define RECOVERY_IMAGE_FILE "recovery.img"
#define TEE1_IMAGE_FILE 	"trustzone.bin"
#define TEE2_IMAGE_FILE 	"trustzone.bin"
#define CUSTOM_IMAGE_FILE   "custom.img"


#define BOOT_DELTA_PARTITION_NAME       "boot"
#define SYSTEM_DELTA_PARTITION_NAME     "system"
#define RECOVERY_DELTA_PARTITION_NAME   "recovery"
#define TEE1_DELTA_PARTITION_NAME   "tee1"
#define TEE2_DELTA_PARTITION_NAME   "tee2"
#define CUSTOM_DELTA_PARTITION_NAME     "custom"

#define FOTA_TEMP_PATH           "/cache/fota"

#define UPDATE_RESULT_FILE      "/cache/data/com.mediatek.dm/files/updateResult"
//#define DEFAULT_DELTA_FOLDER    "/data/delta"

//#define VERIFY_BOOT_SOURCE          0
//#define VERIFY_BOOT_TARGET          0
//#define VERIFY_SYSTEM_SOURCE        0
//#define VERIFY_SYSTEM_TARGET        0
//#define VERIFY_RECOVERY_SOURCE      0
//#define VERIFY_RECOVERY_TARGET      0
//#define VERIFY_CUSTOM_SOURCE           0
//#define VERIFY_CUSTOM_TARGET           0


//#define REDBEND_FAIL_SAFE_1
//#define REDBEND_FAIL_SAFE_2
#define LIBRARY_V7

// ----------------------------------------------------------------------------
//
// Debug Options
//

//#define FOTA_COMPARE_GOLDEN
//#define FOTA_UPDATE_RECOVERY
//#define FOTA_UI_DEBUG_MESSAGE
//#define FOTA_UI_MESSAGE
#define FOTA_DELETE_DELTA_AFTER_UPGRADE

#ifdef FOTA_UI_DEBUG_MESSAGE
#define LOG_UI       ui->Print
#else
#define LOG_UI(...)
#endif

#ifdef FOTA_UI_MESSAGE
#define UI_PRINT    ui->Print
#else
#define UI_PRINT(...)
#endif


// ----------------------------------------------------------------------------
//
// Partition Name
//

#ifdef MTK_EMMC_SUPPORT
#define BOOT_PART_NAME      "bootimg"
#define SYSTEM_PART_NAME    "android"
#define RECOVERY_PART_NAME  "recovery"
#define TEE1_PART_NAME  	"tee1"
#define TEE2_PART_NAME  	"tee2"
#define DATA_PART_NAME      "usrdata"
#else
#define BOOT_PART_NAME      "boot"
#define SYSTEM_PART_NAME    "system"
#define RECOVERY_PART_NAME  "recovery"
#define TEE1_PART_NAME  	"tee1"
#define TEE2_PART_NAME  	"tee2"
#define DATA_PART_NAME      "userdata"
#endif

#define RECOVERY_PART_NAME  "recovery"
#define TEE1_PART_NAME  	"tee1"
#define TEE2_PART_NAME  	"tee2"
#define BACKUP_PART_NAME    "expdb"
#define CUSTOM_PART_NAME    "custom"

#define MAX_PATH  256  // merge with fota_fs.c

#define NAND_START      0
#define NAND_END        0x20000000

// UPI Operation

#define UPI_OP_SCOUT_UPDATE     0x0
#define UPI_OP_VERIFY_SOURCE    0x1
#define UPI_OP_VERIFY_TARGET    0x2
#define UPI_OP_UPDATE           0x3

#define UPI_WORKING_BUFFER_SIZE     20L * 1024 * 1024
#define UPI_BACKUP_BUFFER_NUM       4

//#define RB_UPI_VERSION  "6,3,9,111"
//#define RB_UPI_VERSION  "6,3,9,125"
//#define RB_UPI_VERSION  "6,3,9,142"
#define RB_UPI_VERSION  "7,0,15,92"
#define RB_UPI_VERSION_9  "9.3.0.29"
#define RB_UPI_VERSION_9_64  "9.4.1.7"


enum FOTA_UPGRADE_MODE {
    FOTA_UPGRADE_MODE_UNKNOWN,
    FOTA_UPGRADE_MODE_DELTA,
    FOTA_UPGRADE_MODE_IMAGE
};

typedef struct PartitionInfo
{
    unsigned int  size;
    unsigned int  write_size;
    unsigned int  erase_size;
} PartitionInfo;

struct FotaGlobals
{
    const struct MtdPartition *boot_partition;
    const struct MtdPartition *system_partition;
    const struct MtdPartition *recovery_partition;
	const struct MtdPartition *tee1_partition;
	const struct MtdPartition *tee2_partition;
    const struct MtdPartition *data_partition;
    const struct MtdPartition *backup_partition;
    const struct MtdPartition *custom_partition;

    PartitionInfo boot_partition_info;
    PartitionInfo system_partition_info;
    PartitionInfo recovery_partition_info;
	PartitionInfo tee1_partition_info;
	PartitionInfo tee2_partition_info;
    PartitionInfo backup_partition_info;
    PartitionInfo custom_partition_info;

    char  *boot_delta_file;
    char  *system_delta_file;
    char  *recovery_delta_file;
	char  *tee1_delta_file;
	char  *tee2_delta_file;
    char  *custom_delta_file;

    const struct MtdPartition  *partition;
    PartitionInfo partition_info;
    char   *delta_file_name;
    char   *delta_part_name;
    FILE   *delta_file;

    bool    has_custom_partition;
	bool	has_tee_partition;

    unsigned long  uProgress;
    unsigned long  uRound;

    enum FOTA_UPGRADE_MODE  mode;
};

struct FotaGlobals  gFota;
unsigned char *upi_working_buffer = NULL;

#ifdef FOTA_UI_MESSAGE
extern RecoveryUI* ui;
#endif

unsigned int BackupBuffer[4] = {
    0,
    0x20000,
    0x40000,
    0x60000
};


// ======================================================================================

static void fota_start(void)
{
    //LOG_UI("[%s]\n", __FUNCTION__);
    LOG_INFO("[%s]", __FUNCTION__);

    memset((void *) &gFota, 0, sizeof(struct FotaGlobals));
}

static void fota_exit(void)
{
    //LOG_UI("[%s]\n", __FUNCTION__);
    LOG_INFO("[%s]", __FUNCTION__);

    if (gFota.boot_delta_file)  {
        free(gFota.boot_delta_file);
        gFota.boot_delta_file = NULL;
    }
    if (gFota.system_delta_file)  {
        free(gFota.system_delta_file);
        gFota.system_delta_file = NULL;
    }
    if (gFota.recovery_delta_file)  {
        free(gFota.recovery_delta_file);
        gFota.recovery_delta_file = NULL;
    }
    if (gFota.custom_delta_file)  {
        free(gFota.custom_delta_file);
        gFota.custom_delta_file = NULL;
    }

    if (gFota.delta_file)  {
        fclose(gFota.delta_file);
        gFota.delta_file = NULL;
    }

    if (upi_working_buffer)  {
        free(upi_working_buffer);
    }
}


static bool check_upi_version(void)
{
    char buf[32];


    if (S_RB_SUCCESS != RB_GetUPIVersion((unsigned char *) buf))  {
        LOG_ERROR("Can't get UPI Version");
        return false;
    }

    LOG_INFO("UPI version : %s\n", buf);

    if (!strcmp(RB_UPI_VERSION, buf) || !strcmp(RB_UPI_VERSION_9, buf) || !strcmp(RB_UPI_VERSION_9_64, buf))  {
        return true;
    }

    LOG_ERROR("UPI version error : %s (%s) or (%s) or (%s)", buf, RB_UPI_VERSION, RB_UPI_VERSION_9, RB_UPI_VERSION_9_64);
    LOG_INFO("UPI version error : %s (%s) or (%s) or (%s)", buf, RB_UPI_VERSION, RB_UPI_VERSION_9, RB_UPI_VERSION_9_64);

    return false;
}

static bool mount_partitions(void)
{
    size_t total_size, erase_size, write_size;


    LOG_INFO("[%s]", __FUNCTION__);

    if (fota_scan_partitions() <= 0)  {
        LOG_ERROR("[%s] error scanning partitions", __FUNCTION__);
        return false;
    }

    LOG_INFO("[%s] Mount DATA partition", __FUNCTION__);
    if (!is_support_gpt()) {
        gFota.data_partition = fota_find_partition_by_name(DATA_PART_NAME);
    } else {
        gFota.data_partition = fota_find_partition_by_name("userdata");
    }
    if (gFota.data_partition == NULL)  {
        LOG_ERROR("[%s] Can't find DATA partition", __FUNCTION__);
        return false;
    }

    LOG_INFO("[%s] Mount BOOT partition", __FUNCTION__);
    gFota.boot_partition = fota_find_partition_by_name(BOOT_PART_NAME);
    if (gFota.boot_partition)  {
        if (fota_partition_info(gFota.boot_partition, &total_size, &erase_size, &write_size) != 0)  {
           LOG_ERROR("[%s] Can't stat BOOT partition", __FUNCTION__);
            return false;
        }
    } else  {
        LOG_ERROR("[%s] Can't find BOOT partition", __FUNCTION__);
        return false;
    }
    gFota.boot_partition_info.size = total_size;
    gFota.boot_partition_info.erase_size = erase_size;
    gFota.boot_partition_info.write_size = write_size;

    LOG_INFO("[%s] BOOT partition : total_size=0x%X, erase_size=0x%X, write_size=0x%X",
                __FUNCTION__, total_size, erase_size, write_size);

    LOG_INFO("[%s] Mount SYSTEM partition", __FUNCTION__);
    gFota.system_partition = fota_find_partition_by_name(SYSTEM_PART_NAME);
    if (gFota.system_partition)  {
        if (fota_partition_info(gFota.system_partition, &total_size, &erase_size, &write_size) != 0)  {
           LOG_ERROR("[%s] Can't stat SYSTEM partition", __FUNCTION__);
            return false;
        }
    } else  {
        LOG_ERROR("Can't find SYSTEM partition : %s", SYSTEM_PART_NAME);
        return false;
    }

    gFota.system_partition_info.size = total_size;
    gFota.system_partition_info.erase_size = erase_size;
    gFota.system_partition_info.write_size = write_size;

    LOG_INFO("[%s] SYSTEM partition : total_size=0x%X, erase_size=0x%X, write_size=0x%X",
                __FUNCTION__, total_size, erase_size, write_size);

    //-----------------------------------------------------------------------------------

    LOG_INFO("[%s] Mount RECOVERY partition", __FUNCTION__);
    gFota.recovery_partition = fota_find_partition_by_name(RECOVERY_PART_NAME);
    if (gFota.recovery_partition)  {
        if (fota_partition_info(gFota.recovery_partition, &total_size, &erase_size, &write_size) != 0)  {
           LOG_ERROR("[%s] Can't stat RECOVERY partition", __FUNCTION__);
            return false;
        }
    } else  {
        LOG_ERROR("[%s] Can't find RECOVERY partition", __FUNCTION__);
        return false;
    }

    gFota.recovery_partition_info.size = total_size;
    gFota.recovery_partition_info.erase_size = erase_size;
    gFota.recovery_partition_info.write_size = write_size;

    LOG_INFO("[%s] RECOVERY partition : total_size=0x%X, erase_size=0x%X, write_size=0x%X",
                __FUNCTION__, total_size, erase_size, write_size);

    //-----------------------------------------------------------------------------------

	LOG_INFO("[%s] Mount TEE1 partition", __FUNCTION__);
    gFota.tee1_partition = fota_find_partition_by_name(TEE1_PART_NAME);
    if (gFota.tee1_partition)  {
        if (fota_partition_info(gFota.tee1_partition, &total_size, &erase_size, &write_size) != 0)  {
            LOG_ERROR("[%s] Can't stat TEE1 partition", __FUNCTION__);
            gFota.has_tee_partition = false;
        }
        else  {
            gFota.has_tee_partition = true;
        }
    } else  {
        LOG_ERROR("[%s] Can't find TEE1 partition", __FUNCTION__);
        gFota.has_tee_partition = false;
    }

    if (gFota.has_tee_partition)  {
        gFota.tee1_partition_info.size = total_size;
        gFota.tee1_partition_info.erase_size = erase_size;
        gFota.tee1_partition_info.write_size = write_size;

        LOG_INFO("[%s] TEE1 partition : total_size=0x%X, erase_size=0x%X, write_size=0x%X",
                __FUNCTION__, total_size, erase_size, write_size);
    }

    //-----------------------------------------------------------------------------------

	LOG_INFO("[%s] Mount TEE2 partition", __FUNCTION__);
    gFota.tee2_partition = fota_find_partition_by_name(TEE2_PART_NAME);
    if (gFota.tee2_partition)  {
        if (fota_partition_info(gFota.tee2_partition, &total_size, &erase_size, &write_size) != 0)  {
            LOG_ERROR("[%s] Can't stat TEE2 partition", __FUNCTION__);
            gFota.has_tee_partition = false;
        }
        else  {
            gFota.has_tee_partition = true;
        }
    } else  {
        LOG_ERROR("[%s] Can't find TEE2 partition", __FUNCTION__);
        gFota.has_tee_partition = false;
    }

    if (gFota.has_tee_partition)  {
        gFota.tee2_partition_info.size = total_size;
        gFota.tee2_partition_info.erase_size = erase_size;
        gFota.tee2_partition_info.write_size = write_size;

        LOG_INFO("[%s] TEE2 partition : total_size=0x%X, erase_size=0x%X, write_size=0x%X",
                __FUNCTION__, total_size, erase_size, write_size);
    }

    //-----------------------------------------------------------------------------------

    LOG_INFO("[%s] Mount Custom partition", __FUNCTION__);
    gFota.custom_partition = fota_find_partition_by_name(CUSTOM_PART_NAME);
    if (gFota.custom_partition)  {
        if (fota_partition_info(gFota.custom_partition, &total_size, &erase_size, &write_size) != 0)  {
            LOG_ERROR("[%s] Can't stat CUSTOM partition", __FUNCTION__);
            gFota.has_custom_partition = false;
        }
        else  {
            gFota.has_custom_partition = true;
        }
    } else  {
        LOG_ERROR("[%s] Can't find CUSTOM partition", __FUNCTION__);
        gFota.has_custom_partition = false;
    }

    if (gFota.has_custom_partition)  {
        gFota.custom_partition_info.size = total_size;
        gFota.custom_partition_info.erase_size = erase_size;
        gFota.custom_partition_info.write_size = write_size;

        LOG_INFO("[%s] CUSTOM partition : total_size=0x%X, erase_size=0x%X, write_size=0x%X",
                __FUNCTION__, total_size, erase_size, write_size);
    }

    //-------------------------------------------------------------------------

    //LOG_INFO("[%s] Mount UBOOT partition", __FUNCTION__);
    //gFota.uboot_partition = fota_find_partition_by_name(UBOOT_PART_NAME);
    //if (gFota.uboot_partition)  {
    //    if (fota_partition_info(gFota.uboot_partition, &total_size, &erase_size, &write_size) != 0)  {
    //       LOG_ERROR("[%s] Can't stat UBOOT partition", __FUNCTION__);
    //        return false;
    //    }
    //} else  {
    //    LOG_ERROR("[%s] Can't find UBOOT partition", __FUNCTION__);
    //    return false;
    //}
    //gFota.uboot_partition_info.size = total_size;
    //gFota.uboot_partition_info.erase_size = erase_size;
    //gFota.uboot_partition_info.write_size = write_size;

    LOG_INFO("[%s] Mount BACKUP partition", __FUNCTION__);
    gFota.backup_partition = fota_find_partition_by_name(BACKUP_PART_NAME);
    if (gFota.backup_partition)  {
        if (fota_partition_info(gFota.backup_partition, &total_size, &erase_size, &write_size) != 0)  {
           LOG_ERROR("[%s] Can't stat BACKUP partition", __FUNCTION__);
            return false;
        }
    } else  {
        LOG_ERROR("[%s] Can't find BACKUP partition", __FUNCTION__);
        return false;
    }

    gFota.backup_partition_info.size = total_size;
    gFota.backup_partition_info.erase_size = erase_size;
    gFota.backup_partition_info.write_size = write_size;

    //-------------------------------------------------------------------------

    if (gFota.boot_partition_info.erase_size != gFota.system_partition_info.erase_size)  {
        LOG_ERROR("boot erase size != system erase size");
        return false;
    }

    LOG_INFO("Partition Info");
    //LOG_INFO("  UBOOT : %X %X",
    //    gFota.uboot_partition_info.size, gFota.uboot_partition_info.erase_size, gFota.uboot_partition_info.write_size);
    LOG_INFO("  Boot     : %X %X %X",
        gFota.boot_partition_info.size, gFota.boot_partition_info.erase_size, gFota.boot_partition_info.write_size);
    LOG_INFO("  System   : %X %X %X",
        gFota.system_partition_info.size, gFota.system_partition_info.erase_size, gFota.system_partition_info.write_size);
    LOG_INFO("  Recovery : %X %X %X",
        gFota.recovery_partition_info.size, gFota.recovery_partition_info.erase_size, gFota.recovery_partition_info.write_size);
    LOG_INFO("  Backup   : %X %X %X",
        gFota.backup_partition_info.size, gFota.backup_partition_info.erase_size, gFota.backup_partition_info.write_size);

	if(gFota.has_tee_partition) {
		LOG_INFO("  TEE1 : %X %X %X",
        gFota.tee1_partition_info.size, gFota.tee1_partition_info.erase_size, gFota.tee1_partition_info.write_size);

		LOG_INFO("  TEE2 : %X %X %X",
        gFota.tee2_partition_info.size, gFota.tee2_partition_info.erase_size, gFota.tee2_partition_info.write_size);
	}

    return true;
}

static bool is_file_exist(const char *folder, const char *file)
{
    struct stat st;
    char  path[MAX_PATH];

    strcpy(path, folder);
    strcat(path, file);

    if (lstat(path, &st) < 0) {
        LOG_INFO("[%s] %s not exist", __FUNCTION__, path);
        return false;
    }

    return true;
}

static bool find_delta_update_file(const char *folder)
{
    int  folder_len = strlen(folder);

    if (is_file_exist(folder, BOOT_DELTA_FILE))  {
        gFota.boot_delta_file = (char *) malloc(folder_len + strlen(BOOT_DELTA_FILE) + 1);
        strcpy(gFota.boot_delta_file, folder);
        strcat(gFota.boot_delta_file, BOOT_DELTA_FILE);
    }

    if (is_file_exist(folder, SYSTEM_DELTA_FILE))  {
        gFota.system_delta_file = (char *) malloc(folder_len + strlen(SYSTEM_DELTA_FILE) + 1);
        strcpy(gFota.system_delta_file, folder);
        strcat(gFota.system_delta_file, SYSTEM_DELTA_FILE);
    }

    if (is_file_exist(folder, RECOVERY_DELTA_FILE))  {
        gFota.recovery_delta_file = (char *) malloc(folder_len + strlen(RECOVERY_DELTA_FILE) + 1);
        strcpy(gFota.recovery_delta_file, folder);
        strcat(gFota.recovery_delta_file, RECOVERY_DELTA_FILE);
    }

    if (is_file_exist(folder, TEE1_DELTA_FILE))  {
        gFota.recovery_delta_file = (char *) malloc(folder_len + strlen(TEE1_DELTA_FILE) + 1);
        strcpy(gFota.recovery_delta_file, folder);
        strcat(gFota.recovery_delta_file, TEE1_DELTA_FILE);
    }

    if (is_file_exist(folder, TEE2_DELTA_FILE))  {
        gFota.recovery_delta_file = (char *) malloc(folder_len + strlen(TEE2_DELTA_FILE) + 1);
        strcpy(gFota.recovery_delta_file, folder);
        strcat(gFota.recovery_delta_file, TEE2_DELTA_FILE);
    }
	  
    if (is_file_exist(folder, CUSTOM_DELTA_FILE))  {
        gFota.custom_delta_file = (char *) malloc(folder_len + strlen(CUSTOM_DELTA_FILE) + 1);
        strcpy(gFota.custom_delta_file, folder);
        strcat(gFota.custom_delta_file, CUSTOM_DELTA_FILE);
    }

    if (!(gFota.boot_delta_file || gFota.system_delta_file || gFota.recovery_delta_file || gFota.custom_delta_file || gFota.tee1_delta_file || gFota.tee2_delta_file))  {
        LOG_ERROR("[%s] Can not find any update files", __FUNCTION__);
        return false;
    }

    return true;
}

static bool find_all_update_file(const char *root_path)
{
    if (!root_path)  {
        return false;
    }

    if (ensure_path_mounted(root_path)) {
        LOG_ERROR("[%s] Can't mount %s", __FUNCTION__, root_path);
        return false;
    }

    // ----------------------------------------------------------------------------

    char  folder[MAX_PATH];

    strcpy(folder, root_path);
    if (root_path[strlen(root_path) - 1] != '/')  {
        strcat(folder, "/");
    }

    if (find_delta_update_file(folder))  {
        gFota.mode = FOTA_UPGRADE_MODE_DELTA;
    } else  {
#if 1 //wschen 2012-07-24
        ensure_path_unmounted(root_path);
#endif
        LOG_ERROR("[%s] Can not find any update files", __FUNCTION__);
        return false;
    }

    LOG_INFO("[%s] boot delta = %s", __FUNCTION__, gFota.boot_delta_file);
    LOG_INFO("[%s] system delta = %s", __FUNCTION__, gFota.system_delta_file);
    LOG_INFO("[%s] recovery delta = %s", __FUNCTION__, gFota.recovery_delta_file);
    LOG_INFO("[%s] custom delta = %s", __FUNCTION__, gFota.custom_delta_file);
    LOG_INFO("[%s] tee1 delta = %s", __FUNCTION__, gFota.tee1_delta_file);
    LOG_INFO("[%s] tee2 delta = %s", __FUNCTION__, gFota.tee2_delta_file);
	
    return true;
}

static int fota_update_delta(int operation)
{
    if (!gFota.delta_file_name)  {
        LOG_ERROR("[%s] Can't find %s", __FUNCTION__, gFota.delta_file_name);
        return E_RB_FAILURE;
    }

    long result = 0;

    LOG_INFO("[%s] %d %s", __FUNCTION__, operation, gFota.delta_part_name);

#if 0	//vRM 7.0

    unsigned short partition_name[MAX_PATH] = { '\0' };
	convert_char_to_unicode(gFota.delta_part_name, partition_name);

	unsigned short temp_path[MAX_PATH] = {'\0'};
	unsigned short delta_path[MAX_PATH] = {'\0'};
	unsigned short mount_point[MAX_PATH] = {'\0'};

    convert_char_to_unicode(FOTA_TEMP_PATH, temp_path);
    convert_char_to_unicode(gFota.delta_file_name, delta_path);
	convert_char_to_unicode("/", mount_point);

    CustomerPartitionData partition_data;
    vRM_DeviceData device_data;

    unsigned long InstallerTypes[1] = { 0L };
    unsigned char upi_supplementary_info[1] = { 0 };
    unsigned long upi_supplementary_info_size = 1;

	//setting the 1st structure - partition
    partition_data.partition_name           = (unsigned short *) &partition_name[0];
    partition_data.base_partition_name      = partition_name;
    partition_data.sector_size              = gFota.partition_info.erase_size; //0x20000;
    partition_data.page_size                = gFota.partition_info.erase_size; //0x20000;
    partition_data.rom_start_address        = NAND_START;   // 0;
    partition_data.rom_end_address          = NAND_END;     // 0x20000000;
    partition_data.dir_tree_offset          = 0;
    partition_data.mount_point              = mount_point;
    partition_data.ui16StrSourcePath        = 0;
    partition_data.ui16StrTargetPath        = 0;
	partition_data.ui16StrSourceFileAttr    = 0;
	partition_data.ui16StrTargetFileAttr    = 0;
    partition_data.partition_type           = PT_FOTA;
    partition_data.file_system_type         = FS_JOURNALING_RW;
    partition_data.rom_type                 = ROM_TYPE_EMPTY;
	partition_data.compression_type	        = NO_COMPRESSION;

	//setting the 2nd structure - device data
	device_data.ui32Operation               = operation;    // UPI_OP_UPDATE; //0;
	device_data.ui32DeviceCaseSensitive     = 1;            // case sensitive
	device_data.pRam                        = upi_working_buffer;
	device_data.ui32RamSize                 = UPI_WORKING_BUFFER_SIZE;
    device_data.ui32NumberOfBuffers         = 4;
    device_data.pBufferBlocks			    = (unsigned long *) (&BackupBuffer[0]);
	device_data.ui32NumberOfPartitions      = 1;            // 1 for FOTA
	device_data.pFirstPartitionData         = &partition_data;
	device_data.ui32NumberOfLangs           = 0;            // 0 for FOTA
	device_data.pLanguages				    = 0;            // NULL for FOTA
    device_data.pTempPath				    = temp_path;    // "/data/fota";
	device_data.pSupplementaryInfo          = (unsigned char **) &upi_supplementary_info;
	device_data.pSupplementaryInfoSize      = &upi_supplementary_info_size;
	device_data.pComponentInstallerTypes    = InstallerTypes;
	device_data.ui32ComponentInstallerTypesNum = 1;
	device_data.enmUpdateType			    = UT_NO_SELF_UPDATE;
	device_data.ui32Flags				    = 0;
	device_data.pDeltaPath				    = delta_path;   // "/data/boot.delta";
	device_data.pbUserData				    = 0;
#else	//vRM 9.0

	char partition_name[MAX_PATH] = { '\0' };
	convert_unicode_to_char(gFota.delta_part_name, partition_name);
	
	char temp_path[MAX_PATH] = {'\0'};
	char delta_path[MAX_PATH] = {'\0'};
	char mount_point[MAX_PATH] = {'\0'};
	
	convert_unicode_to_char(FOTA_TEMP_PATH, temp_path);
	convert_unicode_to_char(gFota.delta_file_name, delta_path);
	convert_unicode_to_char("/", mount_point);
	
	CustomerPartitionData partition_data;
	vRM_DeviceData device_data;
	
	unsigned int InstallerTypes[1] = { 0L };
	unsigned int upi_supplementary_info[1] = { 0 };
	unsigned int upi_supplementary_info_size = 1;

	//setting the 1st structure - partition
    partition_data.partition_name           = &partition_name[0];
    partition_data.rom_start_address        = NAND_START;   // 0;
    partition_data.mount_point              = mount_point;
    partition_data.strSourcePath            = 0;
    partition_data.strTargetPath            = 0;
    partition_data.partition_type           = PT_FOTA;


	//setting the 2nd structure - device data
	device_data.ui32Operation               = operation;    // UPI_OP_UPDATE; //0;
	device_data.pRam                        = upi_working_buffer;
	device_data.ui32RamSize                 = UPI_WORKING_BUFFER_SIZE;
    device_data.ui32NumberOfBuffers         = 4;
    device_data.pBufferBlocks			    = (unsigned int *) (&BackupBuffer[0]);
	device_data.ui32NumberOfPartitions      = 1;            // 1 for FOTA
	device_data.pFirstPartitionData         = &partition_data;
    device_data.pTempPath				    = temp_path;    // "/data/fota";
	device_data.pComponentInstallerTypes    = InstallerTypes;
	device_data.ui32ComponentInstallerTypesNum = 1;
	device_data.enmUpdateType			    = UT_NO_SELF_UPDATE;
	device_data.pDeltaPath				    = delta_path;   // "/data/boot.delta";
	device_data.pbUserData				    = 0;
#endif	

    LOG_INFO("call RB_vRM_Update");
	long ret = RB_vRM_Update(&device_data);
    //UI_PRINT("[%s] ret=0x%lX\n", __FUNCTION__, ret);
	LOG_INFO("Update %s, ret=0x%lX ", gFota.delta_file_name, ret);

	return ret;
}

static bool verify_all_update_file(const char *root_path)
{
    int  ret = INSTALL_ERROR;

    LOG_INFO("verify_all_update_file");


    if (gFota.recovery_delta_file)  {
#ifdef VERIFY_RECOVERY_SOURCE
        gFota.uRound++;
#endif
#ifdef VERIFY_RECOVERY_TARGET
        gFota.uRound++;
#endif
    }

    if (gFota.boot_delta_file)  {
#ifdef VERIFY_BOOT_SOURCE
        gFota.uRound++;
#endif
#ifdef VERIFY_BOOT_TARGET
        gFota.uRound++;
#endif
    }

    if (gFota.system_delta_file)  {
#ifdef VERIFY_SYSTEM_SOURCE
        gFota.uRound++;
#endif
#ifdef VERIFY_SYSTEM_TARGET
        gFota.uRound++;
#endif
    }

    if (gFota.custom_delta_file)  {
#ifdef VERIFY_CUSTOM_SOURCE
        gFota.uRound++;
#endif
#ifdef VERIFY_CUSTOM_TARGET
        gFota.uRound++;
#endif
    }

#if defined(VERIFY_RECOVERY_SOURCE) || defined(VERIFY_RECOVERY_TARGET)
    if (gFota.recovery_delta_file)  {
        gFota.partition = gFota.recovery_partition;
        gFota.partition_info = gFota.recovery_partition_info;
        gFota.delta_file_name = gFota.recovery_delta_file;
        gFota.delta_part_name = strdup(RECOVERY_DELTA_PARTITION_NAME);
        gFota.delta_file = fopen(gFota.recovery_delta_file, "rb");

        if (!gFota.delta_file)
            goto FAIL;

    #if defined(VERIFY_RECOVERY_SOURCE)
        ret = fota_update_delta(UPI_OP_VERIFY_SOURCE);
        if (S_RB_SUCCESS != ret)  {
            LOG_ERROR("[%s] verify recovery source error : 0x%X", __FUNCTION__, ret);
            goto FAIL;
        }
    #endif

    #if defined(VERIFY_RECOVERY_TARGET)
        ret = fota_update_delta(UPI_OP_VERIFY_TARGET);
        if (S_RB_SUCCESS != ret)  {
            LOG_ERROR("[%s] verify recovery target error : 0x%X", __FUNCTION__, ret);
            goto FAIL;
        }
    #endif

        fclose(gFota.delta_file);
        gFota.delta_file = NULL;
    }
#endif


#if defined(VERIFY_BOOT_SOURCE) || defined(VERIFY_BOOT_TARGET)
    if (gFota.boot_delta_file)  {
        gFota.partition = gFota.boot_partition;
        gFota.partition_info = gFota.boot_partition_info;
        gFota.delta_file_name = gFota.boot_delta_file;
        gFota.delta_part_name = strdup(BOOT_DELTA_PARTITION_NAME);
        gFota.delta_file = fopen(gFota.boot_delta_file, "rb");

        if (!gFota.delta_file)
            goto FAIL;

    #if defined(VERIFY_BOOT_SOURCE)
        ret = fota_update_delta(UPI_OP_VERIFY_SOURCE);
        if (S_RB_SUCCESS != ret)  {
            LOG_ERROR("[%s] verify boot source error : 0x%X", __FUNCTION__, ret);
            goto FAIL;
        }
    #endif

    #if defined(VERIFY_BOOT_TARGET)
        ret = fota_update_delta(UPI_OP_VERIFY_TARGET);
        if (S_RB_SUCCESS != ret)  {
            LOG_ERROR("[%s] verify boot target error : 0x%X", __FUNCTION__, ret);
            goto FAIL;
        }
    #endif

        fclose(gFota.delta_file);
        gFota.delta_file = NULL;
    }
#endif


#if defined(VERIFY_SYSTEM_SOURCE) || defined(VERIFY_SYSTEM_TARGET)
    if (gFota.system_delta_file)  {
        gFota.partition = gFota.system_partition;
        gFota.partition_info = gFota.system_partition_info;
        gFota.delta_file_name = gFota.system_delta_file;
        gFota.delta_part_name = strdup(SYSTEM_DELTA_PARTITION_NAME);
        gFota.delta_file = fopen(gFota.system_delta_file, "rb");

        if (!gFota.delta_file)
            goto FAIL;

    #if defined(VERIFY_SYSTEM_SOURCE)
        ret = fota_update_delta(UPI_OP_VERIFY_SOURCE);
        if (S_RB_SUCCESS != ret)  {
            LOG_ERROR("[%s] verify system source error : 0x%X", __FUNCTION__, ret);
            goto FAIL;
        }
    #endif
    #if defined(VERIFY_SYSTEM_TARGET)
        ret = fota_update_delta(UPI_OP_VERIFY_TARGET);
        if (S_RB_SUCCESS != ret)  {
            LOG_ERROR("[%s] verify system target error : 0x%X", __FUNCTION__, ret);
            goto FAIL;
        }
    #endif

        fclose(gFota.delta_file);
        gFota.delta_file = NULL;
    }
#endif

#if defined(VERIFY_CUSTOM_SOURCE) || defined(VERIFY_CUSTOM_TARGET)
    if (gFota.custom_delta_file)  {
        gFota.partition = gFota.custom_partition;
        gFota.partition_info = gFota.custom_partition_info;
        gFota.delta_file_name = gFota.custom_delta_file;
        gFota.delta_part_name = strdup(CUSTOM_DELTA_PARTITION_NAME);
        gFota.delta_file = fopen(gFota.custom_delta_file, "rb");

        if (!gFota.delta_file)
            goto FAIL;

    #if defined(VERIFY_CUSTOM_SOURCE)
        ret = fota_update_delta(UPI_OP_VERIFY_SOURCE);
        if (S_RB_SUCCESS != ret)  {
            LOG_ERROR("[%s] verify custom source error : 0x%X", __FUNCTION__, ret);
            goto FAIL;
    }
#endif
    #if defined(VERIFY_CUSTOM_TARGET)
        ret = fota_update_delta(UPI_OP_VERIFY_TARGET);
        if (S_RB_SUCCESS != ret)  {
            LOG_ERROR("[%s] verify custom target error : 0x%X", __FUNCTION__, ret);
            goto FAIL;
        }
    #endif

        fclose(gFota.delta_file);
        gFota.delta_file = NULL;
    }
#endif

    return true;

FAIL:

    if (gFota.delta_file)  {
        fclose(gFota.delta_file);
        gFota.delta_file = NULL;
    }

    return false;
}


#ifdef FOTA_DELETE_DELTA_AFTER_UPGRADE
static bool remove_file(const char *root_path, const char *delta_file)
{
    int  ret = 0;
    char path[MAX_PATH];

    strcpy(path, root_path);
    strcat(path, delta_file);

    LOG_INFO("[%s] %s\n", __FUNCTION__, path);

    ret = unlink(path);

    if (ret == 0)
		return true;

	if (ret < 0 && errno == ENOENT)	//if file does not exist then we can say that we deleted it successfully
		return true;

    return false;
}
#endif

void remove_fota_delta_files(const char *root_path)
{
#ifdef FOTA_DELETE_DELTA_AFTER_UPGRADE

    char path[MAX_PATH];


    strcpy(path, root_path);
    if (root_path[strlen(root_path) - 1] != '/')  {
        strcat(path, "/");
    }

    LOG_INFO("[%s] root_path=%s, path=%s, %d%d%d%d\n",
            __FUNCTION__, root_path, path,
            (gFota.boot_delta_file) ? 1 : 0,
            (gFota.system_delta_file) ? 1 : 0,
            (gFota.recovery_delta_file) ? 1 : 0,
            (gFota.tee1_delta_file) ? 1 : 0,
            (gFota.tee2_delta_file) ? 1 : 0,
            (gFota.custom_delta_file) ? 1 : 0);

    //if (gFota.boot_delta_file)  {
        remove_file(path, BOOT_DELTA_FILE);
    //}
    //if (gFota.system_delta_file)  {
        remove_file(path, SYSTEM_DELTA_FILE);
    //}
    //if (gFota.recovery_delta_file)  {
        remove_file(path, RECOVERY_DELTA_FILE);
    //}
        remove_file(path, TEE1_DELTA_FILE);

	    remove_file(path, TEE2_DELTA_FILE);
	
        remove_file(path, CUSTOM_DELTA_FILE);

        unlink(path);

#else

    return;

#endif
}

#if 0
static void write_result_file(int result)
{
    int result_fd = open(UPDATE_RESULT_FILE, O_RDWR | O_CREAT, 0644);

    if (result_fd < 0) {
        LOG_ERROR("[%s] cannot open '%s' for output", __FUNCTION__, UPDATE_RESULT_FILE);
        return;
    }

    char buf[4];
    if (S_RB_SUCCESS == result)
        strcpy(buf, "1");
    else
        strcpy(buf, "0");
    write(result_fd, buf, 1);
    close(result_fd);
}
#endif

int find_fota_delta_package(const char *root_path)
{
    if (find_all_update_file(root_path))  {
        return 1;
    }

    return 0;
}


extern void write_all_log(void);

#ifdef FOTA_SELF_UPGRADE
static int fota_self_update(void)
{
    int  ret = INSTALL_ERROR;

    if (!gFota.recovery_delta_file && !gFota.tee1_delta_file)  {
        return ret;
    }

    gFota.uRound = 1;

    gFota.partition = gFota.recovery_partition;
    gFota.partition_info = gFota.recovery_partition_info;
    gFota.delta_file_name = gFota.recovery_delta_file;
    gFota.delta_part_name = strdup(RECOVERY_DELTA_PARTITION_NAME);
    gFota.delta_file = fopen(gFota.recovery_delta_file, "rb");

    if (gFota.delta_file) {
        //update recovery
        ret = fota_update_delta(UPI_OP_SCOUT_UPDATE);
        free(gFota.delta_part_name);
        fclose(gFota.delta_file);
	gFota.delta_file = NULL;
    }
		
    //update TEE1 if supported
    if (gFota.tee1_delta_file)  {
	ret = INSTALL_ERROR;
		
        gFota.partition = gFota.tee1_partition;
        gFota.partition_info = gFota.tee1_partition_info;
        gFota.delta_file_name = gFota.tee1_delta_file;
        gFota.delta_part_name = strdup(TEE1_DELTA_PARTITION_NAME);
        gFota.delta_file = fopen(gFota.tee1_delta_file, "rb");

        if (!gFota.delta_file)
            goto FAIL;

        ret = fota_update_delta(UPI_OP_SCOUT_UPDATE);

        free(gFota.delta_part_name);
        fclose(gFota.delta_file);
        gFota.delta_file = NULL;
    }

    if (S_RB_SUCCESS == ret)  {

    #ifdef SUPPORT_SBOOT_UPDATE
        sec_update(false);
    #endif

#ifdef FOTA_SELF_UPGRADE_REBOOT
        ret = unlink(gFota.recovery_delta_file);
    	LOG_INFO("[%s] unlink value: %d, errno: %d", __FUNCTION__, ret, errno);
    	if ((ret == 0) || (ret < 0 && errno == ENOENT))  {
		    // log
        }
        else  {
            LOG_ERROR("[%s] Can not delete %s", __FUNCTION__, gFota.recovery_delta_file);
        }

        fota_exit();

        write_all_log();

        sync();
        android_reboot(ANDROID_RB_RESTART, 0, 0);
#endif

        return ret;
    }

FAIL:

    //fota_exit()

    LOG_ERROR("[%s] self upgrade fail : 0x%X", __FUNCTION__, ret);

    return ret;
}
#endif  // FOTA_SELF_UPGRADE

static int fota_main_update(void)
{
    int  ret = INSTALL_ERROR;

    if (gFota.boot_delta_file)  {
        gFota.partition = gFota.boot_partition;
        gFota.partition_info = gFota.boot_partition_info;
        gFota.delta_file_name = gFota.boot_delta_file;
        gFota.delta_part_name = strdup(BOOT_DELTA_PARTITION_NAME);
        gFota.delta_file = fopen(gFota.boot_delta_file, "rb");

        if (!gFota.delta_file)
            goto FAIL;

        ret = fota_update_delta(UPI_OP_SCOUT_UPDATE);

        free(gFota.delta_part_name);
        fclose(gFota.delta_file);
        gFota.delta_file = NULL;

        if (S_RB_SUCCESS != ret)  {
            LOG_ERROR("[%s] update boot error : 0x%X", __FUNCTION__, ret);
            goto FAIL;
        }
    }


	//Update TEE2 if supported
	if (gFota.tee2_delta_file)	{
			gFota.partition = gFota.tee2_partition;
			gFota.partition_info = gFota.tee2_partition_info;
			gFota.delta_file_name = gFota.tee2_delta_file;
			gFota.delta_part_name = strdup(TEE2_DELTA_PARTITION_NAME);
			gFota.delta_file = fopen(gFota.tee2_delta_file, "rb");
	
			if (!gFota.delta_file)
				goto FAIL;
	
			ret = fota_update_delta(UPI_OP_SCOUT_UPDATE);
	
			free(gFota.delta_part_name);
			fclose(gFota.delta_file);
			gFota.delta_file = NULL;
	
			if (S_RB_SUCCESS != ret)  {
				LOG_ERROR("[%s] update tee2 error : 0x%X", __FUNCTION__, ret);
				goto FAIL;
			}
	}
	

    if (gFota.system_delta_file)  {
        if (ensure_path_mounted("/system") == -1)  {
            LOG_INFO("[%s] can not mount system partition", __FUNCTION__);
            goto FAIL;
        }

        gFota.partition = gFota.system_partition;
        gFota.partition_info = gFota.system_partition_info;
        gFota.delta_file_name = gFota.system_delta_file;
        gFota.delta_part_name = strdup(SYSTEM_DELTA_PARTITION_NAME);
        gFota.delta_file = fopen(gFota.system_delta_file, "rb");

        if (!gFota.delta_file)
            goto FAIL;

        ret = fota_update_delta(UPI_OP_SCOUT_UPDATE);

        free(gFota.delta_part_name);
        fclose(gFota.delta_file);
        gFota.delta_file = NULL;

        if (S_RB_SUCCESS != ret)  {
            LOG_ERROR("[%s] update system error : 0x%X", __FUNCTION__, ret);
            goto FAIL;
        }
    }

    if (gFota.custom_delta_file)  {
        if (ensure_path_mounted("/custom") == -1)  {
            LOG_INFO("[%s] can not mount custom partition", __FUNCTION__);
            goto FAIL;
        }

        gFota.partition = gFota.custom_partition;
        gFota.partition_info = gFota.custom_partition_info;
        gFota.delta_file_name = gFota.custom_delta_file;
        gFota.delta_part_name = strdup(CUSTOM_DELTA_PARTITION_NAME);
        gFota.delta_file = fopen(gFota.custom_delta_file, "rb");

        if (!gFota.delta_file)
            goto FAIL;

        ret = fota_update_delta(UPI_OP_SCOUT_UPDATE);

        free(gFota.delta_part_name);
        fclose(gFota.delta_file);
        gFota.delta_file = NULL;

        if (S_RB_SUCCESS != ret)  {
            LOG_ERROR("[%s] update custom error : 0x%X", __FUNCTION__, ret);
            goto FAIL;
        }
    }

FAIL:

    //fota_exit()

    return ret;
}

#if 0
static void create_self_upgrade_script(const char *root_path)
{
    if (ensure_path_mounted("/system") == -1)  {
        LOG_ERROR("Can't mount /system");
        return;
    }

    int fp = open("/system/etc/install-recovery.sh", O_WRONLY | O_CREAT, 0755);

    if (fp == -1)  {
        LOG_ERROR("Can't create /system/etc/install-recovery.sh");
        return;
    }

    char buf[512];
    memset(buf, 0x00, 512);

    strcat(buf, "#!/system/bin/sh\n");
    strcat(buf, "/system/bin/fota1 --fota_delta_path=");
    strcat(buf, root_path);
    strcat(buf, " --reboot_to_recovery\n");
    write(fp, buf, strlen(buf));

    close(fp);
}
#endif

int install_fota_delta_package(const char *root_path)
{
    int  ret = INSTALL_ERROR;


    fota_start();

    if (!check_upi_version())  {
        goto FAIL;
    }

    if (!mount_partitions())  {
        goto FAIL;
    }

    if (!find_all_update_file(root_path))  {
        goto FAIL;
    }

    if (gFota.recovery_delta_file)  {
        gFota.uRound++;
    }

    if (gFota.boot_delta_file)  {
        gFota.uRound++;
    }

    if (gFota.system_delta_file)  {
        gFota.uRound++;
    }

    if (gFota.custom_delta_file)  {
        gFota.uRound++;
    }

	if (gFota.tee1_delta_file)  {
        gFota.uRound++;
    }

	if (gFota.tee2_delta_file)  {
        gFota.uRound++;
    }

    upi_working_buffer = (unsigned char *) malloc(UPI_WORKING_BUFFER_SIZE);
    if (0 == upi_working_buffer)  {
        LOG_ERROR("Can not alloc working buffer");
        goto FAIL;
    }

    if (!verify_all_update_file(root_path))  {
        goto FAIL;
    }

#ifdef FOTA_UI_MESSAGE
    ui->SetBackground(RecoveryUI::INSTALLING_UPDATE);
    ui->SetProgressType(RecoveryUI::EMPTY);
    ui->ShowProgress(1.0, 0);
#endif

    if (gFota.mode == FOTA_UPGRADE_MODE_DELTA)  {

        if (gFota.recovery_delta_file || gFota.tee1_delta_file)  {
            #ifdef SUPPORT_SBOOT_UPDATE    
                char  sec_ver1_path[MAX_PATH];
                strcpy(sec_ver1_path, root_path);
                if (root_path[strlen(root_path) - 1] != '/'){
                    strcat(sec_ver1_path, "/SEC_VER1.txt");
                }else{
                    strcat(sec_ver1_path, "SEC_VER1.txt");
                }
                if(0 != (ret=sec_verify_img_info_fota(sec_ver1_path,false)))
                {
                    LOG_ERROR("[%s] verify %s image info error : 0x%X", __FUNCTION__, sec_ver1_path, ret);
                }
                ret = sec_mark_status(false);
                LOG_INFO("[%s] s_mark_status , ret:%x", __FUNCTION__,ret);
            #endif
            #ifdef FOTA_SELF_UPGRADE
                ret = fota_self_update();
            #else
                ret = INSTALL_SUCCESS;
            #endif
        }else {
            #ifdef SUPPORT_SBOOT_UPDATE    
                char  sec_ver2_path[MAX_PATH];
                strcpy(sec_ver2_path, root_path);
                if (root_path[strlen(root_path) - 1] != '/'){
                    strcat(sec_ver2_path, "/SEC_VER2.txt");
                }else{
                    strcat(sec_ver2_path, "SEC_VER2.txt");
                }
                if(0 != (ret=sec_verify_img_info_fota(sec_ver2_path,false)))
                {
                    LOG_ERROR("[%s] verify %s image info error : 0x%X", __FUNCTION__, sec_ver2_path, ret);
                }
                ret = sec_mark_status(false);
                LOG_INFO("[%s] s_mark_status , ret:%x", __FUNCTION__,ret);
            #endif
            #ifdef FOTA_PHONE_UPGRADE
                ret = fota_main_update();
            #else
                ret = INSTALL_SUCCESS;
            #endif
        }
    }
    else if (gFota.mode == FOTA_UPGRADE_MODE_IMAGE)  {
        // not support
    }

FAIL:
    fota_exit();

    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// NAND Interface
//

/* RB_GetBlockSize() - Returns the size of one memory block.
 *
 * This function must be called before any block reading or writing can be done.
 * Note: For a given update, the sequence of blocks that is being updated is always the same.
 * This assumption is made in order to be able to successfully continue update after failure situation.
 */
long RB_GetBlockSize(void *pbUserData)
{
    LOG_INFO("[%s] : %d", __FUNCTION__, gFota.partition_info.erase_size);

    return gFota.partition_info.erase_size;
}


/* RB_ReadImage() - Reads data from flash.
 *
 * Number of bytes to read should be less or equal to block size.
 * The data is used to create new block based on read data and diff file information
 */
long RB_ReadImage(
	void *pbUserData,					/* User data passed to all porting routines */
	unsigned char *pbBuffer,			/* pointer to user buffer */
    RB_UINT32 dwStartAddress,		/* Location in Flash */
    RB_UINT32 dwSize) 				/* number of bytes to copy */
{
    LOG_INFO("[%s] from 0x%X to 0x%X, size = 0x%X", __FUNCTION__, dwStartAddress, (int) pbBuffer, dwSize);

    if (dwStartAddress >= gFota.partition_info.size)  {
        LOG_ERROR("[%s] exceed the partition size : %X %X", __FUNCTION__, dwStartAddress, gFota.partition_info.size);
        return E_RB_BAD_PARAMS;
    }

    LOG_INFO("[%s] addr=0x%X", __FUNCTION__, dwStartAddress);

    memset(pbBuffer, 0xAA, dwSize);

    MtdReadContext *in = fota_read_partition(gFota.partition);

    if (in == NULL)  {
        LOG_ERROR("[%s] can not read partition", __FUNCTION__);
        return E_RB_FAILURE;
    }

#ifdef MTK_EMMC_SUPPORT
    ssize_t size = fota_read_data_ex(in, (char *) pbBuffer, dwSize, dwStartAddress);
    if (size != (ssize_t) dwSize)  {
        LOG_ERROR("[%s] align read len error : %X %X", __FUNCTION__, size, dwSize);
        fota_read_close(in);
        return E_RB_FAILURE;
    }
#else
    unsigned long block_addr = dwStartAddress & (~0x1FFFF);

    if (block_addr == dwStartAddress)  {
        ssize_t size = fota_read_data_ex(in, (char *) pbBuffer, dwSize, dwStartAddress);
        if (size != (ssize_t) dwSize)  {
            LOG_ERROR("[%s] align read len error : %X %X", __FUNCTION__, size, dwSize);
            return E_RB_FAILURE;
        }
    }
    else  {
        char tmp_buf[0x20000];
        unsigned long offset = dwStartAddress - block_addr;
        unsigned long prev_read = 0x20000 - offset;

        //               offset          prev_read
        //        |------------------+--------------|
        //     block_addr           addr

        LOG_INFO("[%s] unalign read : 0x%X 0x%X 0x%X %X", __FUNCTION__,
                 dwStartAddress, block_addr, offset, prev_read);

        ssize_t size = fota_read_data_ex(in, tmp_buf, 0x20000, block_addr);
        if (size != (ssize_t) 0x20000)  {
            LOG_ERROR("[%s] unalign read len error : %X %X", __FUNCTION__, size, 0x20000);
            return E_RB_FAILURE;
        }
        memcpy((char *) pbBuffer, tmp_buf + offset, prev_read);

        if (dwSize > prev_read)  {
            size = fota_read_data_ex(in, (char *) pbBuffer + prev_read, dwSize - prev_read, dwStartAddress+prev_read);
            if (size != (ssize_t) (dwSize - prev_read))  {
                LOG_ERROR("[%s] unalign read len error : %X %X", __FUNCTION__, size, dwSize - prev_read);
                return E_RB_FAILURE;
            }
        }
    }
#endif

    fota_read_close(in);

#ifdef FOTA_COMPARE_GOLDEN
    FILE *fp = fopen("/data/boot_1048p27_phone.img", "rb");
    unsigned char  *pTmp = malloc(dwSize);
    int  i;
    if (fp && pTmp)  {
        fseek(fp, addr, SEEK_SET);
        fread(pTmp, sizeof(char), dwSize, fp);
        for (i = 0; i < dwSize; i++)  {
            if (pbBuffer[i] != pTmp[i])
                break;
        }
        if (i != dwSize)  {
            LOG_ERROR("[%s] Compare Golden Fail : 0x%X.", __FUNCTION__, i);
            LOG_ERROR("[%s] Y : %X %X %X %X %X %X %X %X", __FUNCTION__,
                        pTmp[i+0], pTmp[i+1], pTmp[i+2], pTmp[i+3],
                        pTmp[i+4], pTmp[i+5], pTmp[i+6], pTmp[i+7]);
            LOG_ERROR("[%s] N : %X %X %X %X %X %X %X %X", __FUNCTION__,
                        pbBuffer[i+0], pbBuffer[i+1], pbBuffer[i+2], pbBuffer[i+3],
                        pbBuffer[i+4], pbBuffer[i+5], pbBuffer[i+6], pbBuffer[i+7]);
        }
        else  {
            LOG_INFO("[%s] Compare Golden OK.", __FUNCTION__);
        }
    }
    else   {
        LOG_ERROR("[%s] Can not open golden file.", __FUNCTION__);
    }

    if (pTmp)   free(pTmp);
    if (fp)     fclose(fp);
#endif  // FOTA_COMPARE_GOLDEN

    if (dwSize > 16)  {
        LOG_HEX("[RB_ReadImage] ", (const char *) pbBuffer, 16);
        LOG_HEX("[RB_ReadImage] ", (const char *) (pbBuffer + dwSize - 16), 16);
    } else  {
        LOG_HEX("[RB_ReadImage] ", (const char *) pbBuffer, dwSize);
    }

    return S_RB_SUCCESS;
}

extern void write_all_log(void);

static void FOTA_ReadBlock(unsigned long dwBlockAddr)
{
    unsigned char *pbBuf;
    size_t  readLen, expectLen;

    LOG_INFO("[%s] addr=0x%X, size=0x%X", __FUNCTION__,
            dwBlockAddr, gFota.partition_info.erase_size);

    pbBuf = (unsigned char *) malloc(gFota.partition_info.erase_size);

    MtdReadContext *in = fota_read_partition(gFota.partition);

    if (!in)  {
        LOG_INFO("[%s] error", __FUNCTION__);
        return;
    }

    expectLen = gFota.partition_info.erase_size;
    readLen = fota_read_data_ex(in, (char *) pbBuf, expectLen, dwBlockAddr);
    if (expectLen != readLen)  {
        LOG_ERROR("[%s] Can not read enough data 0x%X 0x%X", __FUNCTION__, expectLen, readLen);
    }

    fota_read_close(in);

    LOG_HEX("[FOTA_ReadBlock]+ ", (const char *) pbBuf, 64);
    LOG_HEX("[FOTA_ReadBlock]- ", (const char *) (pbBuf + readLen - 64), 64);

    free(pbBuf);
}

/* RB_WriteBlock() - Writes one block to a given location in flash.
 *
 * Erases the image location and writes the data to that location
 */
long RB_WriteBlock(
	void *pbUserData,					/* User data passed to all porting routines */
	RB_UINT32 dwBlockAddress,		/* address of the block to be updated */
	unsigned char *pbBuffer) 			/* pointer to data to be written */

{
    long ret = E_RB_FAILURE;

    LOG_INFO("[%s] addr=0x%X, size=0x%X", __FUNCTION__,
            dwBlockAddress, gFota.partition_info.erase_size);

    if (pbBuffer == NULL)  {
        LOG_ERROR("[%s] no data", __FUNCTION__);
        return E_RB_BAD_PARAMS;
    }

    if (dwBlockAddress % gFota.partition_info.erase_size)  {
        LOG_ERROR("[%s] address not block alignment", __FUNCTION__);
        return E_RB_BAD_PARAMS;
    }

    if (dwBlockAddress >= gFota.partition_info.size)  {
        LOG_ERROR("[%s] exceed the partition size : %X %X", __FUNCTION__, dwBlockAddress, gFota.partition_info.size);
        return E_RB_BAD_PARAMS;
    }

    LOG_HEX("[RB_WriteBlock] ", (const char *) pbBuffer, 16);

#ifdef FOTA_COMPARE_GOLDEN
    FILE *fp = fopen("/data/boot_1048nr9_phone.img", "rb");
    size_t size = gFota.partition_info.erase_size;
    unsigned char  *pTmp = malloc(size);
    if (fp && pTmp)  {
        fseek(fp, addr, SEEK_SET);
        fread(pTmp, sizeof(char), size, fp);
        if (memcmp(pbBuffer, pTmp, size) != 0) {
            LOG_ERROR("[%s] Compare Golden Fail.", __FUNCTION__);
        }
        else  {
            LOG_INFO("[%s] Compare Golden OK.", __FUNCTION__);
        }
    }
    else   {
        LOG_ERROR("[%s] Can not open golden file.", __FUNCTION__);
    }

    if (pTmp)   free(pTmp);
    if (fp)     fclose(fp);
#endif  // FOTA_COMPARE_GOLDEN


    MtdWriteContext *out = fota_write_partition(gFota.partition);

    if (out == NULL)  {
        LOG_ERROR("[%s] fota_write_partition fail", __FUNCTION__);
        return E_RB_FAILURE;
    }

#ifdef MTK_EMMC_SUPPORT
    if (-1 == fota_write_data_ex(out, (char *) pbBuffer, gFota.partition_info.write_size, dwBlockAddress))  {
        LOG_ERROR("RB_WriteBlock Error");
        //fota_write_close(out);
        ret = E_RB_FAILURE;
    }
    else  {
        ret = S_RB_SUCCESS;
    }
#else
    if (-1 == fota_write_block_ex(out, (char *) pbBuffer, dwBlockAddress))  {
        LOG_ERROR("RB_WriteBlock Error");
        return E_RB_FAILURE;
    }
    else  {
        ret = S_RB_SUCCESS;
    }
#endif

    fota_write_close(out);

#ifdef REDBEND_FAIL_SAFE_1
    if (!recovery_update)  {
        if (dwBlockAddress == 0x100000)  {
            FOTA_ReadBlock(0xE0000);
            write_all_log();
            android_reboot(0xDEAD0001, 0, 0);
        }
    }
#endif

    return ret;
}

/* RB_WriteMetadataOfBlock() - Write one metadata of a given block in flash
 *
 * writes the metadata in the matching place according to the represented block location
 */
long RB_WriteMetadataOfBlock(
	void *pbUserData,					/* User data passed to all porting routines */
	unsigned long dwBlockAddress,		/* address of the block of that the metadata describe */
	unsigned long dwMdSize,				/* Size of the data that the metadata describe*/
	unsigned char *pbMDBuffer) 			/* pointer to metadata to be written */
{
    LOG_INFO("[%s]", __FUNCTION__);
    LOG_ERROR("%s : addr = 0x%X, size = 0x%X", __FUNCTION__, dwBlockAddress, dwMdSize);

    // Unused in Redbend solution.

    return E_RB_FAILURE;
}

/* RB_GetDelta() - Get the Delta either as a whole or in block pieces */
long RB_GetDelta(
	void *pbUserData,				    /* User data passed to all porting routines */
	unsigned char *pbBuffer,			/* pointer to user buffer */
    RB_UINT32 dwStartAddressOffset, /* offset from start of delta file */
    RB_UINT32 dwSize)               /* buffer size in bytes */
{

    LOG_INFO("[%s] 0x%X 0x%X",  __FUNCTION__, dwStartAddressOffset, dwSize);

    fseek(gFota.delta_file, dwStartAddressOffset, SEEK_SET);
    int n = ftell(gFota.delta_file);
    if (n != (int) dwStartAddressOffset)  {
        LOG_ERROR("[%s] Can not move file pointer : 0x%X 0x%X", __FUNCTION__, n, (int) dwStartAddressOffset);
        return E_RB_FAILURE;
    }

    n = fread(pbBuffer, sizeof(char), dwSize, gFota.delta_file);
    if (n != (int) dwSize) {
        LOG_ERROR("[%s] Can not read enough data %X", __FUNCTION__, n);
        return E_RB_FAILURE;
    }

    if (dwSize > 32)  {
        LOG_HEX("[RB_GetDelta] ", (const char *) pbBuffer, 32);
        LOG_HEX("[RB_GetDelta] ", (const char *) (pbBuffer + dwSize - 32), 32);
    } else  {
        LOG_HEX("[RB_GetDelta] ", (const char *) pbBuffer, dwSize);
    }

    return S_RB_SUCCESS;
}

#ifdef LIBRARY_V7
long RB_GetRBDeltaOffset(void *pbUserData, RB_UINT32 signed_delta_offset, RB_UINT32* delta_offset)
{
    LOG_INFO("[%s] signed_delta_offset=0x%X", signed_delta_offset);

	*delta_offset = signed_delta_offset;

	return 0;
}
#else
long RB_GetRBDeltaOffset(
    void *pbUserData,
    unsigned long delta_ordinal,
    unsigned long* delta_offset,
    unsigned long *installer_types,
    unsigned long installer_types_num,
    UpdateType update_type)
{
    LOG_INFO("[%s] delta_ordinal=0x%X, delta_offset=0x%X, installer_types=0x%X, installer_types_num=0x%X, update_type=%d",
        __FUNCTION__, delta_ordinal, *delta_offset, *installer_types, installer_types_num, update_type);

    // RB default
    unsigned long size = 0;
    return RB_GetSignedDeltaOffset(delta_ordinal, delta_offset, &size, installer_types, installer_types_num, update_type);
}
#endif

/* RB_ReadBackupBlock() - Copy data from backup block to RAM.
 *
 * Can copy data of specified size from any location in one of the buffer blocks and into specified RAM location
 */
long RB_ReadBackupBlock(
	void *pbUserData,					/* User data passed to all porting routines */
	unsigned char *pbBuffer,			/* pointer to user buffer in RAM where the data will be copied */
	RB_UINT32 dwBlockAddress,		/* address of data to read into RAM. Must be inside one of the backup buffer blocks */
	RB_UINT32 dwSize)				/* buffer size in bytes */
{
    LOG_INFO("[%s] addr = 0x%lX, size = 0x%lX", __FUNCTION__, dwBlockAddress, dwSize);

    if (!gFota.backup_partition)  {
        LOG_ERROR("[%s] No backup partition", __FUNCTION__);
        return E_RB_FAILURE;
    }

    if (dwBlockAddress % gFota.backup_partition_info.erase_size)  {
        LOG_ERROR("[%s] address not block alignment", __FUNCTION__);
    }

    MtdReadContext *in = fota_read_partition(gFota.backup_partition);

    if (in == NULL)  {
        LOG_ERROR("[%s] can not read partition", __FUNCTION__);
    }

    ssize_t size = fota_read_data_ex(in, (char *) pbBuffer, dwSize, dwBlockAddress);
    if (size != (ssize_t) dwSize)  {
        LOG_ERROR("[%s] align read len error : %X %X", __FUNCTION__, size, dwSize);
        fota_read_close(in);
        return E_RB_FAILURE;
    }

    fota_read_close(in);

    return S_RB_SUCCESS;
}

/* RB_WriteBackupBlock() - Copy data from specified address in RAM to a backup block.
 *
 * This function copies data from specified address in RAM to a backup buffer block.
 * This will always write a complete block (sector) and the address will always be one of the addresses
*  provided in RB_ImageUpdate() call.
 */
long RB_WriteBackupBlock(
    void *pbUserData,					/* User data passed to all porting routines */
    RB_UINT32 dwBlockStartAddress,	/* address of the block to be updated */
    unsigned char *pbBuffer)  	        /* RAM to copy data from */
{
    long ret = E_RB_FAILURE;

    LOG_INFO("[%s] addr = 0x%lX", __FUNCTION__, dwBlockStartAddress);

    if (!gFota.backup_partition)  {
        LOG_ERROR("[%s] No backup partition", __FUNCTION__);
        return E_RB_FAILURE;
    }

    if (dwBlockStartAddress % gFota.backup_partition_info.erase_size)  {
        LOG_ERROR("[%s] address not block alignment", __FUNCTION__);
    }

    MtdWriteContext *out = fota_write_partition(gFota.backup_partition);

    if (out == NULL)  {
        LOG_ERROR("[%s] fota_write_partition fail", __FUNCTION__);
        return E_RB_FAILURE;
    }

    if (-1 == fota_write_data_ex(out, (char *) pbBuffer, gFota.backup_partition_info.write_size, dwBlockStartAddress))  {
        LOG_ERROR("RB_WriteBlock Error");
        ret = E_RB_FAILURE;
    }
    else  {
        ret = S_RB_SUCCESS;
    }

    fota_write_close(out);

    return ret;
}

/* RB_EraseBackupBlock() - Erase a specific backup block.
 *
 * This function will erase the specified block starting from dwStartAddress.  The
 * block address must be aligned to sector size and a single sector should be erased.
 */
long RB_EraseBackupBlock(
    void *pbUserData,					/* User data passed to all porting routines */
    RB_UINT32 dwStartAddress)		/* block start address in flash to erase */
{
    LOG_ERROR("[%s]", __FUNCTION__);
    LOG_INFO("[%s]", __FUNCTION__);

    // Unused function

    return S_RB_SUCCESS;
}

/* RB_WriteBackupPartOfBlock() - Copy data from specified address in RAM to part of a backup block.
 *
 * This function copies data from specified address in RAM to a backup buffer block.
 * This will write part of a block (sector) that was already written with RB_WriteBackupBlock().
 */
long RB_WriteBackupPartOfBlock(
    void *pbUserData,					/* User data passed to all porting routines */
    RB_UINT32 dwStartAddress,		/* Start address in flash to write to */
    RB_UINT32 dwSize,				/* Size of data to write */
    unsigned char* pbBuffer)			/* Buffer in RAM to write from */
{
    long ret = E_RB_FAILURE;

    LOG_INFO("[%s] addr = 0x%lX, size = 0x%lX", __FUNCTION__, dwStartAddress, dwSize);
    //LOG_UI("[%s] addr = 0x%lX, size = 0x%lX", __FUNCTION__, dwStartAddress, dwSize);

    if (!gFota.backup_partition)  {
        LOG_ERROR("[%s] No backup partition", __FUNCTION__);
        return E_RB_FAILURE;
    }

    if (dwStartAddress % gFota.backup_partition_info.erase_size)  {
        LOG_ERROR("[%s] address not block alignment", __FUNCTION__);
    }

    MtdWriteContext *out = fota_write_partition(gFota.backup_partition);

    if (out == NULL)  {
        LOG_ERROR("[%s] fota_write_partition fail", __FUNCTION__);
        return E_RB_FAILURE;
    }

    if (-1 == fota_write_data_ex(out, (char *) pbBuffer, gFota.backup_partition_info.write_size, dwStartAddress))  {
        LOG_ERROR("RB_WriteBlock Error");
        fota_write_close(out);
        ret = E_RB_FAILURE;
    }
    else  {
        ret = S_RB_SUCCESS;
    }

    fota_write_close(out);

    return ret;
}

/* RB_ResetTimerA() - Reset watchdog timer A
 *
 * This function is being called periodically within the 30-second period
 */
long RB_ResetTimerA(void)
{
	LOG_INFO("[%s]", __FUNCTION__);
	return S_RB_SUCCESS;
}

/* RB_Trace() - Display trace messages on the console for debug purposes
 *
 * Format and print data using the C printf() format
 */
RB_UINT32 RB_Trace(
    void *pbUserData,                   /* User data passed to all porting routines */
    const char *aFormat,...)            /* format string to printf */
{
    int err = errno;
    va_list args;
    va_start(args, aFormat);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), aFormat, args);
    va_end(args);

    fprintf(stdout, "[%s] %s", __FUNCTION__, buf);
    clearerr(stdout);
    fflush(stdout);

    return S_RB_SUCCESS;
}

/* RB_Progress() - Report the current stage of the update
 *
 * Provides information in percents on the update process progress
 */
void RB_Progress(
    void *pbUserData,           /* User data passed to all porting routines */
    RB_UINT32 uPercent)     /* progress info in percents */
{
    float fPercent;

    fPercent = (float) (gFota.uProgress + uPercent) / ((float) gFota.uRound * 100.0);

    LOG_INFO("[%s] %ld %ld %f", __FUNCTION__, uPercent, gFota.uProgress, fPercent);

#ifdef FOTA_UI_MESSAGE
    ui->SetProgress(fPercent);
#endif

    if (uPercent == 100)  {
        gFota.uProgress += 100;
    }
}

long RB_GetAvailableFreeSpace(void *pbUserData, const char* partition_name, RB_UINT32* available_flash_size)
{
    char name[MAX_PATH] = {'\0'};

    convert_unicode_to_char(partition_name, name);

    struct statfs st;

    if (statfs(name, &st) < 0) {
        LOG_ERROR("[%s] Can not get system stat (%s)", __FUNCTION__, strerror(errno));
        *available_flash_size = 0xEFFFFFFF;
    } else {
        LOG_INFO("[%s] bfree=0x%llX, bsize=0x%llX", __FUNCTION__, st.f_bfree, st.f_bsize);
        *available_flash_size = st.f_bfree * st.f_bsize ;
    }

    LOG_INFO("[%s] %s, ret = 0x%lX", __FUNCTION__, name, *available_flash_size);

    return S_RB_SUCCESS;
}


// Not in Spec
long RB_LockFile(unsigned short* file_full_path)
{
    LOG_INFO("[%s]", __FUNCTION__);
    return 0;
}

long RB_UnlockFile(unsigned short* file_full_path)
{
    LOG_INFO("[%s]", __FUNCTION__);
    return 0;
}

#if 0
long RB_GetDPProtocolVersion(void* pbUserData, void* pbyRAM, unsigned long dwRAMSize,
							 unsigned long *installer_types, unsigned long installer_types_num, unsigned long component_flags,
							 unsigned long *dpProtocolVersion)
{
    LOG_INFO("[%s]", __FUNCTION__);
    return 0;
}


long RB_GetDPScoutProtocolVersion(void* pbUserData, void* pbyRAM, unsigned long dwRAMSize,
								  unsigned long *installer_types, unsigned long installer_types_num, unsigned long component_flags,
								  unsigned long *dpScoutProtocolVersion)
{
    LOG_INFO("[%s]", __FUNCTION__);
    return 0;
}
#endif

RB_RETCODE RB_ReadSourceBytes(
	void *pbUserData,				/* User data passed to all porting routines. Could be NULL */
    unsigned long address,			/* address of page in flash to retrieve */
    unsigned long size,				/* size of page in flash to retrieve */
    unsigned char *buff,			/* buffer large enough to contain size bytes */
	long section_id)
{
    LOG_INFO("[%s]", __FUNCTION__);
    return 0;
}

long RB_ReadImageNewKey(
	void *pbUserData,					/* User data passed to all porting routines */
	unsigned char *pbBuffer,
	RB_UINT32 dwStartAddress,
	RB_UINT32 dwSize)
{
    LOG_INFO("[%s] pbBuffer=%p, addr=0x%X, size=0x%X", __FUNCTION__, pbBuffer, dwStartAddress, dwSize);

    return RB_ReadImage(pbUserData, pbBuffer, dwStartAddress, dwSize);
}

long RB_ResetTimerB(void)
{
    LOG_INFO("[%s]", __FUNCTION__);
    return 0;
}

#if 0
long RB_SemaphoreInit(void *pbUserData, RB_SEMAPHORE semaphore, long num_resources)
{
    LOG_INFO("[%s]", __FUNCTION__);
    return 0;
}

long RB_SemaphoreDestroy(void *pbUserData, RB_SEMAPHORE semaphore)
{
    LOG_INFO("[%s]", __FUNCTION__);
    return 0;
}

long RB_SemaphoreDown(void *pbUserData, RB_SEMAPHORE semaphore,long num)
{
    LOG_INFO("[%s]", __FUNCTION__);
    return 0;
}

long RB_SemaphoreUp(void *pbUserData, RB_SEMAPHORE semaphore,long num)
{
    LOG_INFO("[%s]", __FUNCTION__);
    return 0;
}

void *RB_Malloc(RB_UINT32 size)
{
    LOG_INFO("[%s]", __FUNCTION__);
    return 0;
}

void RB_Free(void *pMemBlock)
{
    LOG_INFO("[%s]", __FUNCTION__);
}

#endif


#ifdef __cplusplus
}
#endif	/* __cplusplus */
