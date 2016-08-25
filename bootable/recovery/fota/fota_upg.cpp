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
#define TEE1_DELTA_FILE     "tee1.delta"
#define TEE2_DELTA_FILE     "tee2.delta"
#define CUSTOM_DELTA_FILE   "custom.delta"


#define FOTA_TEMP_PATH           "/cache/fota"

#define UPDATE_RESULT_FILE      "/cache/data/com.mediatek.dm/files/updateResult"

#define VERIFY_BOOT_SOURCE          false
#define VERIFY_BOOT_TARGET          false
#define VERIFY_SYSTEM_SOURCE        false
#define VERIFY_SYSTEM_TARGET        false
#define VERIFY_RECOVERY_SOURCE      false
#define VERIFY_RECOVERY_TARGET      false
#define VERIFY_TEE1_SOURCE          false
#define VERIFY_TEE1_TARGET          false
#define VERIFY_TEE2_SOURCE          false
#define VERIFY_TEE2_TARGET          false
#define VERIFY_CUSTOM_SOURCE        false
#define VERIFY_CUSTOM_TARGET        false

//#define REDBEND_FAIL_SAFE_1

// ----------------------------------------------------------------------------
//
// Debug Options
//

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

#define BOOT_PARTITION_NAME      	"/boot"
#define SYSTEM_PARTITION_NAME    	"/system"
#define RECOVERY_PARTITION_NAME  	"/recovery"
#define TEE1_PARTITION_NAME	        "/tee1"
#define TEE2_PARTITION_NAME             "/tee2"
#define DATA_PARTITION_NAME      	"/data"
#define CUSTOM_PARTITION_NAME    	"/custom"

#define MAX_PATH  256  // merge with fota_fs.c

#define NAND_START      0
#define NAND_END        0x20000000

// UPI Operation

#define UPI_OP_SCOUT_UPDATE     0x0
#define UPI_OP_VERIFY_SOURCE    0x1
#define UPI_OP_VERIFY_TARGET    0x2
#define UPI_OP_UPDATE           0x3

#define UPI_WORKING_BUFFER_SIZE     32L * 1024 * 1024
#define UPI_BACKUP_BUFFER_NUM       4

#define DEFAULT_BLOCK_SIZE      0x20000

//#define RB_UPI_VERSION  "6,3,9,111"
//#define RB_UPI_VERSION  "6,3,9,125"
//#define RB_UPI_VERSION  "6,3,9,142"
#define RB_UPI_VERSION  "7,0,15,92"
#define RB_UPI_VERSION_9  "9.3.0.29"
#define RB_UPI_VERSION_9_64  "9.4.1.7"


enum FOTA_PARTITION  {
    FOTA_BOOT_PARTITION,
    FOTA_SYSTEM_PARTITION,
    FOTA_RECOVERY_PARTITION,
    FOTA_TEE1_PARTITION,
    FOTA_TEE2_PARTITION,
    FOTA_CUSTOM_PARTITION,
    FOTA_DATA_PARTITION,
    FOTA_PARTITION_MAX
};

char  PartitionName[FOTA_PARTITION_MAX][32] = {
    "BOOT",      // FOTA_BOOT_PARTITION,
    "SYSTEM",    // FOTA_SYSTEM_PARTITION,
    "RECOVERY",  // FOTA_RECOVERY_PARTITION,
    "TEE1",      // FOTA_TEE1_PARTITION,
    "TEE2",      // FOTA_TEE2_PARTITION,
    "CUSTOM",    // FOTA_CUSTOM_PARTITION,
    "DATA",      // FOTA_DATA_PARTITION,
};

typedef struct PartitionInfo
{
    char          name[MAX_PATH];
    char          dev_name[MAX_PATH];
    unsigned int  size;

    bool          need_verify_source;
    bool          need_verify_target;
    bool          need_mount;

    char*         delta_name;
    int           delta_fd;
} PartitionInfo;

PartitionInfo gPartInfo[FOTA_PARTITION_MAX] = {
    { BOOT_PARTITION_NAME,     BOOT_PART,     0, VERIFY_BOOT_SOURCE,     VERIFY_BOOT_TARGET,     false,  NULL, -1 },  // FOTA_BOOT_PARTITION,
    { SYSTEM_PARTITION_NAME,   SYSTEM_PART,   0, VERIFY_SYSTEM_SOURCE,   VERIFY_SYSTEM_TARGET,   true,   NULL, -1 },  // FOTA_SYSTEM_PARTITION,
    { RECOVERY_PARTITION_NAME, RECOVERY_PART, 0, VERIFY_RECOVERY_SOURCE, VERIFY_RECOVERY_TARGET, false,  NULL, -1 },  // FOTA_RECOVERY_PARTITION,
    { TEE1_PARTITION_NAME,     TEE1_PART,     0, VERIFY_TEE1_SOURCE,     VERIFY_TEE1_TARGET,     false,  NULL, -1 },  // FOTA_TEE1_PARTITION,
    { TEE2_PARTITION_NAME,     TEE2_PART,     0, VERIFY_TEE2_SOURCE,     VERIFY_TEE2_TARGET,     false,  NULL, -1 },  // FOTA_TEE2_PARTITION,
    { CUSTOM_PARTITION_NAME,   CUSTOM_PART,   0, VERIFY_CUSTOM_SOURCE,   VERIFY_CUSTOM_TARGET,   true,   NULL, -1 },  // FOTA_CUSTOM_PARTITION,
    { DATA_PARTITION_NAME,     DATA_PART,     0, false,                  false,                  false,  NULL, -1 }   // FOTA_DATA_PARTITION,
};

unsigned long   uFotaProgress;
unsigned long   uFotaRound;
unsigned char*  upi_working_buffer = NULL;

#ifdef FOTA_UI_MESSAGE
extern RecoveryUI* ui;
#endif

unsigned long BackupBuffer[4] = {
    0,
    0x20000,
    0x40000,
    0x60000
};


// ======================================================================================

static void fota_start(void)
{
    LOG_INFO("[%s]", __FUNCTION__);

    uFotaProgress = 0;
    uFotaRound = 0;
    upi_working_buffer = NULL;
};

static void fota_exit(void)
{
    LOG_INFO("[%s]", __FUNCTION__);

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

    return false;
}

static int atoh(const char *s)
{
    int  value = 0, result = 0;
    char c;

    while ((c = *s++) != NULL) {
        if (c >= '0' && c <= '9')
            value = (int) (c - '0');
        else if (c >= 'a' && c <= 'f')
            value = (int) (c - 'a') + 10;
        else if (c >= 'A' && c <= 'F')
            value = (int) (c - 'A') + 10;
        else
            break;

        result = (result << 4) + value;
    }

    return result;
}

static void get_partition_size(void)
{
    FILE* fp;
    char  buf[512];
    char  p_name[32], p_size[32], p_addr[32];
    int   i;

    LOG_INFO("[%s]\n", __FUNCTION__);

    fp = fopen("/proc/partinfo", "r");
    if (!fp)  {
        LOG_INFO("[%s] Can't open /proc/partinfo", __FUNCTION__);
        return;
    }

    if (fgets(buf, sizeof(buf), fp) != NULL)  {
        while (fgets(buf, sizeof(buf), fp)) {
            if (sscanf(buf, "%s %s %s", p_name, p_addr, p_size) == 3) {
                for (i = 0; i < FOTA_PARTITION_MAX; i++)  {
                    if (!strcmp(gPartInfo[i].name+1, p_name))  {
                        gPartInfo[i].size = atoh(p_size+2);
                    }
                }
            }
        }
    }
    fclose(fp);
}

static bool mount_partitions(void)
{
    LOG_INFO("[%s]", __FUNCTION__);

    load_volume_table();

    for (int i = 0; i < FOTA_PARTITION_MAX; i++)  {
        char *part_name = gPartInfo[i].name;

        LOG_INFO("[%s] Mount %s", __FUNCTION__, part_name);

        Volume *v = volume_for_path(part_name);
        if (v)  {
            gPartInfo[i].size = v->length;
        } else  {
            LOG_ERROR("[%s] Can't find %s", __FUNCTION__, part_name);
        }
    }

    get_partition_size();

    LOG_INFO("Partition Info");
    for (int i = 0; i < FOTA_PARTITION_MAX; i++)  {
        LOG_INFO("%s %s 0x%X",
                    gPartInfo[i].name, gPartInfo[i].dev_name, gPartInfo[i].size);
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

    int  i;

    for (i = 0; i < FOTA_PARTITION_MAX; i++)  {
        gPartInfo[i].delta_name = (char *) malloc(MAX_PATH);
        strcpy(gPartInfo[i].delta_name, folder);
    }

    strcat(gPartInfo[FOTA_BOOT_PARTITION].delta_name, BOOT_DELTA_FILE);
    strcat(gPartInfo[FOTA_SYSTEM_PARTITION].delta_name, SYSTEM_DELTA_FILE);
    strcat(gPartInfo[FOTA_RECOVERY_PARTITION].delta_name, RECOVERY_DELTA_FILE);
    strcat(gPartInfo[FOTA_TEE1_PARTITION].delta_name, TEE1_DELTA_FILE);
    strcat(gPartInfo[FOTA_TEE2_PARTITION].delta_name, TEE2_DELTA_FILE);
    strcat(gPartInfo[FOTA_CUSTOM_PARTITION].delta_name, CUSTOM_DELTA_FILE);

    struct stat st;
    bool  has_delta = false;

    for (i = 0; i < FOTA_PARTITION_MAX; i++)  {
        if ((lstat(gPartInfo[i].delta_name, &st) < 0) || !S_ISREG(st.st_mode))  {
            LOG_INFO("[%s] %s not exist", __FUNCTION__, gPartInfo[i].delta_name);
            free(gPartInfo[i].delta_name);
            gPartInfo[i].delta_name = NULL;
        }
        else  {
            has_delta = true;
        }
    }

    if (!has_delta)  {
        ensure_path_unmounted(root_path);
        LOG_ERROR("[%s] Can not find any update files", __FUNCTION__);
        return false;
    }

    LOG_INFO("[%s] boot delta = %s", __FUNCTION__, gPartInfo[FOTA_BOOT_PARTITION].delta_name);
    LOG_INFO("[%s] system delta = %s", __FUNCTION__, gPartInfo[FOTA_SYSTEM_PARTITION].delta_name);
    LOG_INFO("[%s] recovery delta = %s", __FUNCTION__, gPartInfo[FOTA_RECOVERY_PARTITION].delta_name);
    LOG_INFO("[%s] tee1 delta = %s", __FUNCTION__, gPartInfo[FOTA_TEE1_PARTITION].delta_name);
    LOG_INFO("[%s] tee2 delta = %s", __FUNCTION__, gPartInfo[FOTA_TEE2_PARTITION].delta_name);
    LOG_INFO("[%s] custom delta = %s", __FUNCTION__, gPartInfo[FOTA_CUSTOM_PARTITION].delta_name);

    return true;
}

static int fota_update_delta(FOTA_PARTITION part, int operation)
{
    LOG_UI("fota_update_delta\n");

    long result = 0;

    LOG_INFO("[%s] %d %s", __FUNCTION__, operation, gPartInfo[part].delta_name);
    LOG_UI("[%s] %d %s\n", __FUNCTION__, operation, gPartInfo[part].delta_name);

    gPartInfo[part].delta_fd = open(gPartInfo[part].delta_name, O_RDONLY);
    if (gPartInfo[part].delta_fd == -1)  {
        return E_RB_FAILURE;
    }

#if 0	//vRM7.0
    unsigned short partition_name[MAX_PATH] = { '\0' };
	convert_char_to_unicode(gPartInfo[part].name+1, partition_name);  // skip first /

	unsigned short temp_path[MAX_PATH] = {'\0'};
	unsigned short delta_path[MAX_PATH] = {'\0'};
	unsigned short mount_point[MAX_PATH] = {'\0'};

    convert_char_to_unicode(FOTA_TEMP_PATH, temp_path);
    convert_char_to_unicode(gPartInfo[part].delta_name, delta_path);
	convert_char_to_unicode("/", mount_point);

    CustomerPartitionData partition_data;
    vRM_DeviceData device_data;

    unsigned long InstallerTypes[1] = { 0L };
    unsigned char upi_supplementary_info[1] = { 0 };
    unsigned long upi_supplementary_info_size = 1;

	//setting the 1st structure - partition
    partition_data.partition_name           = (unsigned short *) &partition_name[0];
    partition_data.base_partition_name      = partition_name;
    partition_data.sector_size              = DEFAULT_BLOCK_SIZE; // gPartInfo[part].erase_size; //0x20000;
    partition_data.page_size                = DEFAULT_BLOCK_SIZE; // gPartInfo[part].erase_size; //0x20000;
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
	device_data.pbUserData				    = (void *) &(gPartInfo[part]);
#else	//vRM9.0

    char partition_name[MAX_PATH] = { '\0' };
    convert_unicode_to_char(gPartInfo[part].name+1, partition_name);  // skip first /

    char temp_path[MAX_PATH] = {'\0'};
    char delta_path[MAX_PATH] = {'\0'};
    char mount_point[MAX_PATH] = {'\0'};

    convert_unicode_to_char(FOTA_TEMP_PATH, temp_path);
    convert_unicode_to_char(gPartInfo[part].delta_name, delta_path);
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
    device_data.pTempPath				    = temp_path;    // "/cache/fota";
    device_data.pComponentInstallerTypes    = InstallerTypes;
    device_data.ui32ComponentInstallerTypesNum = 1;
    device_data.enmUpdateType			    = UT_NO_SELF_UPDATE;
    device_data.pDeltaPath				    = delta_path;   // "/cache/boot.delta";
    device_data.pbUserData				    = (void *) &(gPartInfo[part]);
#endif


        LOG_INFO("call RB_vRM_Update");

	long ret = RB_vRM_Update(&device_data);

	LOG_INFO("Update %s, ret=0x%lX ", gPartInfo[part].delta_name, ret);
	LOG_UI("Update %s, ret=0x%lX\n", gPartInfo[part].delta_name, ret);

	close(gPartInfo[part].delta_fd);

	return ret;
}

static bool verify_all_update_file(const char *root_path)
{
    int  ret = INSTALL_ERROR;
    int  i;

    LOG_INFO("verify_all_update_file");

    for (i = 0; i < FOTA_PARTITION_MAX; i++)  {
        if (gPartInfo[i].delta_name)  {
            if (gPartInfo[i].need_verify_source)  {
                ret = fota_update_delta((FOTA_PARTITION) i, UPI_OP_VERIFY_SOURCE);
                if (S_RB_SUCCESS != ret)  {
                    LOG_ERROR("[%s] verify source error : %d => 0x%X", __FUNCTION__, i, ret);
                    //goto FAIL;
                    return false;
                }
            }
            if (gPartInfo[i].need_verify_target)  {
                ret = fota_update_delta((FOTA_PARTITION) i, UPI_OP_VERIFY_TARGET);
                if (S_RB_SUCCESS != ret)  {
                    LOG_ERROR("[%s] verify source error : %d => 0x%X", __FUNCTION__, i, ret);
                    //goto FAIL;
                    return false;
                }
            }
        }
    }

    return true;
}

void remove_fota_delta_files(const char *root_path)
{
#ifdef FOTA_DELETE_DELTA_AFTER_UPGRADE

    char path[MAX_PATH];
    int  ret = 0;

    strcpy(path, root_path);
    if (root_path[strlen(root_path) - 1] != '/')  {
        strcat(path, "/");
    }

    LOG_INFO("[%s] root_path=%s, path=%s, %d%d%d%d\n",
            __FUNCTION__, root_path, path,
            gPartInfo[FOTA_BOOT_PARTITION].delta_name ? 1 : 0,
            gPartInfo[FOTA_SYSTEM_PARTITION].delta_name ? 1 : 0,
            gPartInfo[FOTA_RECOVERY_PARTITION].delta_name ? 1 : 0,
            gPartInfo[FOTA_TEE1_PARTITION].delta_name ? 1 : 0,
            gPartInfo[FOTA_TEE2_PARTITION].delta_name ? 1 : 0,
            gPartInfo[FOTA_CUSTOM_PARTITION].delta_name ? 1 : 0);


    for (int i = 0; i < FOTA_PARTITION_MAX; i++)  {
        if (gPartInfo[i].delta_name)  {

            ret = unlink(gPartInfo[i].delta_name);

            if ((ret == 0) || (ret < 0 && errno == ENOENT))  {
                //if file does not exist then we can say that we deleted it successfully
                LOG_INFO("delte %s", gPartInfo[i].delta_name);
            }
            else  {
                LOG_ERROR("Can't delte %s [0x%X]", gPartInfo[i].delta_name, errno);
            }
        }
    }

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

    if (!gPartInfo[FOTA_RECOVERY_PARTITION].delta_name && !gPartInfo[FOTA_TEE1_PARTITION].delta_name)  {
        return ret;
    }

    uFotaRound = 1;

    if(gPartInfo[FOTA_RECOVERY_PARTITION].delta_name) {
        LOG_INFO("Updating recovery...\n");
        ret = fota_update_delta(FOTA_RECOVERY_PARTITION, UPI_OP_SCOUT_UPDATE);
    }

    if(gPartInfo[FOTA_TEE1_PARTITION].delta_name) {
        LOG_INFO("Updating tee1...\n");
        ret = INSTALL_ERROR;
        ret = fota_update_delta(FOTA_TEE1_PARTITION, UPI_OP_SCOUT_UPDATE);
    }

    if (S_RB_SUCCESS == ret)  {

    #ifdef SUPPORT_SBOOT_UPDATE
        sec_update(false);
    #endif

#ifdef FOTA_SELF_UPGRADE_REBOOT
        //ret = unlink(gFota.recovery_delta_file);
        ret = unlink(gPartInfo[FOTA_RECOVERY_PARTITION].delta_name);
    	LOG_INFO("[%s] unlink value: %d, errno: %d", __FUNCTION__, ret, errno);
    	if ((ret == 0) || (ret < 0 && errno == ENOENT))  {
		    // log
        }
        else  {
            LOG_ERROR("[%s] Can not delete %s", __FUNCTION__, gPartInfo[FOTA_RECOVERY_PARTITION].delta_name);
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
    int  i;

    LOG_UI("fota_main_update");

    for (i = 0; i < FOTA_PARTITION_MAX; i++)  {
        if (gPartInfo[i].delta_name)  {
            if (gPartInfo[i].need_mount)  {
                if (ensure_path_mounted(gPartInfo[i].name) == -1)  {
                    LOG_INFO("[%s] can not mount %s partition", __FUNCTION__, gPartInfo[i].name);
                    goto FAIL;
                }
            }

            ret = fota_update_delta((FOTA_PARTITION) i, UPI_OP_SCOUT_UPDATE);

            if (S_RB_SUCCESS != ret)  {
                LOG_ERROR("[%s] update %s error : 0x%X", __FUNCTION__, PartitionName[i], ret);
                goto FAIL;
            }
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
    int  i;


    fota_start();

    LOG_UI("check_upi_version");

    if (!check_upi_version())  {
        goto FAIL;
    }

    LOG_UI("mount_partitions");

    if (!mount_partitions())  {
        goto FAIL;
    }

    LOG_UI("find_all_update_file");

    if (!find_all_update_file(root_path))  {
        goto FAIL;
    }

    LOG_UI("Count round");

    uFotaRound = 0;
    for (i = 0; i < FOTA_PARTITION_MAX; i++)  {
        if (gPartInfo[i].delta_name)  {
            if (gPartInfo[i].need_verify_source)  {
                uFotaRound++;
            }
            if (gPartInfo[i].need_verify_target)  {
                uFotaRound++;
            }
            uFotaRound++;
        }
    }

    LOG_UI("Allocate working buffer");

    upi_working_buffer = (unsigned char *) malloc(UPI_WORKING_BUFFER_SIZE);
    if (0 == upi_working_buffer)  {
        LOG_ERROR("Can not alloc working buffer");
        goto FAIL;
    }

    LOG_UI("verify_all_update_file");

    if (!verify_all_update_file(root_path))  {
        goto FAIL;
    }

#ifdef FOTA_UI_MESSAGE
    ui->SetBackground(RecoveryUI::INSTALLING_UPDATE);
    ui->SetProgressType(RecoveryUI::EMPTY);
    ui->ShowProgress(1.0, 0);
#endif

    if (gPartInfo[FOTA_RECOVERY_PARTITION].delta_name || gPartInfo[FOTA_TEE1_PARTITION].delta_name)  {
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
    }
    else  {
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

FAIL:
    fota_exit();

    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//

long FOTA_WritePartition(PartitionInfo *part_info, unsigned char *buf, unsigned long addr, unsigned long size)
{
    LOG_INFO("[%s] addr=0x%X, size=0x%X", __FUNCTION__, addr, size);

    if (buf == NULL)  {
        LOG_ERROR("[%s] no data", __FUNCTION__);
        return E_RB_BAD_PARAMS;
    }

    if (addr >= part_info->size)  {
        LOG_ERROR("[%s] exceed the partition size : %X %X", __FUNCTION__, addr, part_info->size);
        return E_RB_BAD_PARAMS;
    }

    LOG_HEX("[FOTA_WritePartition] ", (const char *) buf, 16);

    int fd = open(part_info->dev_name, O_WRONLY | O_SYNC);
    if (fd == -1) {
        LOG_INFO("[%s] Can't open device : %s [%s]",  __FUNCTION__, part_info->name, part_info->dev_name);
        return E_RB_BAD_PARAMS;
    }

    if (lseek(fd, addr, SEEK_SET) == -1)  {
        LOG_INFO("[%s] Can't seek to : 0x%X [%s] ",  __FUNCTION__, addr, part_info->name);
        return E_RB_BAD_PARAMS;
    }

    unsigned long write_size = write(fd, buf, size);
    if (write_size != size)  {
        LOG_INFO("[%s] Can't read 0x%X bytes, only 0x%X [%s] ",  __FUNCTION__, size, write_size, part_info->name);
        return E_RB_BAD_PARAMS;
    }

    close(fd);

    return S_RB_SUCCESS;
}

long FOTA_ReadPartition(PartitionInfo *part_info, unsigned char *buf, unsigned long addr, unsigned long size)
{
    LOG_INFO("[%s] addr = 0x%X, size = 0x%X, buf = 0x%X\n", __FUNCTION__, addr, size, buf);
    LOG_INFO("[%s] %s %s 0x%X\n", __FUNCTION__, part_info->name, part_info->dev_name, part_info->size);

    if (addr >= part_info->size)  {
        LOG_INFO("[%s] exceed the partition size : %X %X", __FUNCTION__, addr, part_info->size);
        return E_RB_BAD_PARAMS;
    }

    int fd = open(part_info->dev_name, O_RDONLY);
    if (fd == -1) {
        LOG_INFO("[%s] Can't open device : %s [%s]",  __FUNCTION__, part_info->name, part_info->dev_name);
        return E_RB_BAD_PARAMS;
    }

    if (lseek(fd, addr, SEEK_SET) == -1)  {
        LOG_INFO("[%s] Can't seek to : 0x%X [%s] ",  __FUNCTION__, addr, part_info->name);
        return E_RB_BAD_PARAMS;
    }

    unsigned long read_size = read(fd, buf, size);
    if (read_size != size)  {
        LOG_INFO("[%s] Can't read 0x%X bytes, only 0x%X [%s] ",  __FUNCTION__, size, read_size, part_info->name);
        return E_RB_BAD_PARAMS;
    }

    close(fd);

    if (size > 16)  {
        LOG_HEX("[FOTA_ReadPartition] ", (const char *) buf, 16);
        LOG_HEX("[FOTA_ReadPartition] ", (const char *) (buf + size - 16), 16);
    } else  {
        LOG_HEX("[FOTA_ReadPartition] ", (const char *) buf, size);
    }

    return S_RB_SUCCESS;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Redbend Interface Function
//

/* RB_GetBlockSize() - Returns the size of one memory block.
 *
 * This function must be called before any block reading or writing can be done.
 * Note: For a given update, the sequence of blocks that is being updated is always the same.
 * This assumption is made in order to be able to successfully continue update after failure situation.
 */
long RB_GetBlockSize(void *pbUserData)
{
    LOG_UI("[%s] 0x%X\n", __FUNCTION__, DEFAULT_BLOCK_SIZE);

    return DEFAULT_BLOCK_SIZE;
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
    PartitionInfo  *part_info = (PartitionInfo *) pbUserData;

    LOG_INFO("[%s] %s %s 0x%X", __FUNCTION__, part_info->name, part_info->dev_name, part_info->size);
    LOG_INFO("[%s] from 0x%X, size = 0x%X", __FUNCTION__, dwStartAddress, dwSize);

    return FOTA_ReadPartition(part_info, pbBuffer, dwStartAddress, dwSize);
}

extern void write_all_log(void);


/* RB_WriteBlock() - Writes one block to a given location in flash.
 *
 * Erases the image location and writes the data to that location
 */
long RB_WriteBlock(
	void *pbUserData,					/* User data passed to all porting routines */
	RB_UINT32 dwBlockAddress,		/* address of the block to be updated */
	unsigned char *pbBuffer) 			/* pointer to data to be written */

{
    PartitionInfo  *part_info = (PartitionInfo *) pbUserData;

    LOG_INFO("[%s] addr=0x%X, size=0x%X", __FUNCTION__, dwBlockAddress, DEFAULT_BLOCK_SIZE);

    if (dwBlockAddress % DEFAULT_BLOCK_SIZE)  {
        LOG_ERROR("[%s] address not block alignment : 0x%X", __FUNCTION__, dwBlockAddress);
        return E_RB_BAD_PARAMS;
    }

    return FOTA_WritePartition(part_info, pbBuffer, dwBlockAddress, DEFAULT_BLOCK_SIZE);
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
    LOG_INFO("[%s] : addr = 0x%X, size = 0x%X", __FUNCTION__, dwBlockAddress, dwMdSize);

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
    PartitionInfo  *part_info = (PartitionInfo *) pbUserData;

    LOG_INFO("[%s] 0x%X 0x%X",  __FUNCTION__, dwStartAddressOffset, dwSize);

    off_t cur_pos = lseek(part_info->delta_fd, dwStartAddressOffset, SEEK_SET);
    if (cur_pos != (off_t) dwStartAddressOffset)  {
        LOG_ERROR("[%s] Can not move file pointer : 0x%X 0x%X", __FUNCTION__, cur_pos, (int) dwStartAddressOffset);
        return E_RB_FAILURE;
    }

    ssize_t read_size = read(part_info->delta_fd, pbBuffer, dwSize);
    if (read_size != (ssize_t) dwSize) {
        LOG_ERROR("[%s] Can not read enough data %X", __FUNCTION__, read_size);
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

long RB_GetRBDeltaOffset(void *pbUserData, RB_UINT32 signed_delta_offset, RB_UINT32* delta_offset)
{
    LOG_INFO("[%s] signed_delta_offset=0x%X", signed_delta_offset);

	*delta_offset = signed_delta_offset;

	return 0;
}

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
    PartitionInfo  *part_info = (PartitionInfo *) pbUserData;

    LOG_INFO("[%s] addr = 0x%lX, size = 0x%lX", __FUNCTION__, dwBlockAddress, dwSize);

    if (dwBlockAddress % DEFAULT_BLOCK_SIZE)  {
        LOG_ERROR("[%s] address not block alignment", __FUNCTION__);
    }

    return FOTA_ReadPartition(part_info, pbBuffer, dwBlockAddress, dwSize);
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

    PartitionInfo  *part_info = (PartitionInfo *) pbUserData;

    LOG_INFO("[%s] addr = 0x%lX", __FUNCTION__, dwBlockStartAddress);

    if (dwBlockStartAddress % DEFAULT_BLOCK_SIZE)  {
        LOG_ERROR("[%s] address not block alignment : 0x%X", __FUNCTION__, dwBlockStartAddress);
        return E_RB_BAD_PARAMS;
    }

    return FOTA_WritePartition(part_info, pbBuffer, dwBlockStartAddress, DEFAULT_BLOCK_SIZE);
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
    PartitionInfo  *part_info = (PartitionInfo *) pbUserData;

    LOG_INFO("[%s] addr = 0x%lX, size = 0x%lX", __FUNCTION__, dwStartAddress, dwSize);

    if (dwStartAddress % DEFAULT_BLOCK_SIZE)  {
        LOG_ERROR("[%s] address not block alignment : 0x%X", __FUNCTION__, dwStartAddress);
        return E_RB_BAD_PARAMS;
    }

    return FOTA_WritePartition(part_info, pbBuffer, dwStartAddress, dwSize);
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

    //fPercent = (float) (gFota.uProgress + uPercent) / ((float) gFota.uRound * 100.0);
    fPercent = (float) (uFotaProgress + uPercent) / ((float) uFotaRound * 100.0);

    LOG_INFO("[%s] %ld %ld %ld %f", __FUNCTION__, uPercent, uFotaProgress, uFotaRound,  fPercent);

#ifdef FOTA_UI_MESSAGE
    ui->SetProgress(fPercent);
#endif

    if (uPercent == 100)  {
        uFotaProgress += 100;
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
