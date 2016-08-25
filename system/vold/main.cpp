/*
* Copyright (C) 2014 MediaTek Inc.
* Modification based on code covered by the mentioned copyright
* and/or permission notice(s).
*/
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <fs_mgr.h>
#include <unistd.h>
#ifndef MTK_EMULATOR_SUPPORT
  #include "libnvram.h"
#endif
#include "CFG_OMADMUSB_File.h"

#undef LOG_TAG
#define LOG_TAG "Vold"

#include "cutils/klog.h"
#include "cutils/log.h"
#include "cutils/properties.h"

#include "VolumeManager.h"
#include "CommandListener.h"
#include "NetlinkManager.h"
#include "DirectVolume.h"
#include "cryptfs.h"
#include "fat_on_nand.h"
#include <semaphore.h>

extern int iFileOMADMUSBLID;

static int process_config(VolumeManager *vm);
static void coldboot(const char *path);
int coldboot_sent_uevent_count=0;
static bool coldboot_sent_uevent_count_only = true;
sem_t coldboot_sem;

#define FSTAB_PREFIX "/fstab."
struct fstab *fstab;


#define MAX_NVRAM_RESTRORE_READY_RETRY_NUM	(20)

int get_boot_mode(void)
{
  int fd;
  ssize_t s;
  char boot_mode[4] = {'0'};
  char boot_mode_sys_path[] = "/sys/class/BOOT/BOOT/boot/boot_mode";

  fd = open(boot_mode_sys_path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  if (fd < 0)
  {
    SLOGE("fail to open %s: %s", boot_mode_sys_path, strerror(errno));
    return 0;
  }

  s = read(fd, boot_mode, sizeof(boot_mode) - 1);
  close(fd);

  if(s <= 0)
  {
    SLOGE("could not read boot mode sys file: %s", strerror(errno));
    return 0;
  }

  boot_mode[s] = '\0';
  return atoi(boot_mode);
}

int is_meta_boot(void)
{
  return (get_boot_mode()==1);  
}

void create_link_for_mtklogger(const char *ext_sd_path)
{
    static char link_path[255];
    if (ext_sd_path == NULL) {
        ext_sd_path = link_path;
    }
    else {      
        strcpy(link_path, ext_sd_path);
    }

    SLOGD("%s(ext_sd_path = %s)", __func__, ext_sd_path);       

    if(get_boot_mode()!=0) {
        SLOGD("This is NOT normal boot.");       

        int ret = -1, round = 3 ;
        while(1) {
            ret = symlink(ext_sd_path, EXT_SDCARD_TOOL); 
            round-- ;

            if(ret) {
                if((round > 0) && (errno == EEXIST)) {
                    SLOGE("The link already exists!");      
                    SLOGE("Try again! round : %d", round);      
                    unlink(EXT_SDCARD_TOOL) ;
                }
                else {
                    SLOGE("Create symlink failed.");        
                    break ;
                }
            }
            else {
                SLOGD("The link is created successfully!");     
                break ;
            }
        } 
    }
    else {
        SLOGD("This is normal boot.");       
        unlink(EXT_SDCARD_TOOL) ;
    }

    return;
}

#ifdef MTK_SDCARDFS
#include <sys/inotify.h>
#include <private/android_filesystem_config.h>

/* Path to system-provided mapping of package name to appIds */
static const char* const kPackagesListFile = "/data/system/packages.list";

static int sdcardfs_read_package_list() {
    //TODO: add lock
    SLOGI("start: sdcardfs_read_package_list \n");

    system("echo 1 > /proc/sdcardfs/clear");

    FILE* file = fopen(kPackagesListFile, "r");
    if (!file) {
        SLOGE("failed to open package list: %s\n", strerror(errno));
        //TODO: add unlock
        return -1;
    }

    char buf[512];
    bool is_found = false;
    while (fgets(buf, sizeof(buf), file) != NULL) {
        char package_name[512];
        int appid;
        char gids[512];
        char proccmd[1024]; //TODO: check overflow ..

        is_found = false;
        if (sscanf(buf, "%s %d %*d %*s %*s %s", package_name, &appid, gids) == 3) {
            char* package_name_dup = strdup(package_name);
           
            SLOGD("package_name_dup: %s  appid:%d\r\n",package_name_dup,appid);
            sprintf(proccmd,"echo %s %d > /proc/sdcardfs/pkguid",package_name_dup,appid);//TODO: use snprintf instead ccyeh
            system(proccmd);

            char* token = strtok(gids, ",");
            while (token != NULL) {
                if (strtoul(token, NULL, 10) == AID_SDCARD_RW) {
                    SLOGD("appid_with_rw: founded, appid:%d @AID_SDCARD_RW \n", appid);
                    sprintf(proccmd,"echo %d > /proc/sdcardfs/rw_main",appid);//TODO: use snprintf instead 
                    system(proccmd);
                }else if(strtoul(token, NULL, 10) == AID_MEDIA_RW){
                    SLOGD("appid_with_rw: founded, appid:%d @AID_MEDIA_RW \n", appid);
                    sprintf(proccmd,"echo %d > /proc/sdcardfs/rw_sub",appid);//TODO: use snprintf instead 
                    system(proccmd);                    
                }
                token = strtok(NULL, ",");
            }
        }
    }

    SLOGI("exit: sdcardfs_read_package_list \n");
    fclose(file);
    //TODO: unlock
    return 0;
}

static void *sdcardfs_watch_package_list(void *ignored)
{
    int ret = 0;
    struct inotify_event *event;
    char         event_buf[512];

    int nfd = inotify_init();
    if (nfd < 0) {
        SLOGE("inotify_init failed: %s\n", strerror(errno));
        ret = -1;
        return (void *)ret;
    }

    bool active = false;
    while (1) {
        if (!active) {
            int res = inotify_add_watch(nfd, kPackagesListFile, IN_DELETE_SELF);
            if (res == -1) {
                if (errno == ENOENT || errno == EACCES) {
                    /* Framework may not have created yet, sleep and retry */
                    SLOGE("missing packages.list; retrying\n");
                    sleep(3);
                    continue;
                } else {
                    SLOGE("inotify_add_watch failed: %s\n", strerror(errno));
                    ret = -1;
                    return (void *)ret;
                }
            }

            /* Watch above will tell us about any future changes, so read the current state. */
            if (sdcardfs_read_package_list() == -1) {
                SLOGE("read_package_list failed: %s\n", strerror(errno));
                ret = -1;
                return (void *)ret;
            }
            active = true;
        }

        int event_pos = 0;
        int res = read(nfd, event_buf, sizeof(event_buf));
        if (res < (int) sizeof(*event)) {
            if (errno == EINTR)
                continue;
            SLOGE("failed to read inotify event: %s\n", strerror(errno));
            ret = -1;
            return (void *)ret;
        }

        while (res >= (int) sizeof(*event)) {
            int event_size;
            event = (struct inotify_event *) (event_buf + event_pos);

            SLOGI("inotify event: %08x\n", event->mask);
            if ((event->mask & IN_IGNORED) == IN_IGNORED) {
                /* Previously watched file was deleted, probably due to move
                           * that swapped in new data; re-arm the watch and read. */
                active = false;
            }

            event_size = sizeof(*event) + event->len;
            res -= event_size;
            event_pos += event_size;
        }
    }

    return (void *)ret;
}
#endif

#ifdef MTK_EMULATOR_SUPPORT
void disable_usb_by_dm(){}
#else
#include "../libfile_op/libfile_op.h"
int NvramAccessForOMADM(OMADMUSB_CFG_Struct *st, bool isRead) {
	if (!st) return -1;
	
	int file_lid = iFileOMADMUSBLID;
	F_ID nvram_fid;
	int rec_size;
	int rec_num;
	int Ret = 0;
	
	nvram_fid = NVM_GetFileDesc(file_lid, &rec_size, &rec_num, isRead);
	if (nvram_fid.iFileDesc < 0) {
		SLOGE("Unable to open NVRAM file!");
		return -1;
	}
	
	if (isRead)	Ret = read(nvram_fid.iFileDesc, st, rec_size*rec_num);//read NVRAM
	else		Ret = write(nvram_fid.iFileDesc, st, rec_size*rec_num);//write NVRAM
	
	if (Ret < 0) {
		SLOGE("access NVRAM error!");
		return -1;
	}

        if (!isRead){
            FileOp_BackupToBinRegionForDM();
        }

	NVM_CloseFileDesc(nvram_fid);
	SLOGD("read st  nvram_fd=%d   Ret=%d  usb=%d  adb=%d  rndis=%d  rec_size=%d  rec_num=%d", nvram_fid.iFileDesc, Ret, st->iUsb, st->iAdb, st->iRndis, rec_size, rec_num);

	return 0;
}

static void *do_disable_usb_by_dm(void *ignored){
    int fd = 0;
    char value[20];
    int count = 0;
    int Ret = 0;
    int rec_size;
    int rec_num;
    int file_lid = iFileOMADMUSBLID;
    OMADMUSB_CFG_Struct mStGet;

    int nvram_restore_ready_retry=0;
    char nvram_init_val[PROPERTY_VALUE_MAX] = {0};
    memset(&mStGet, 0, sizeof(OMADMUSB_CFG_Struct));
    SLOGD("Check whether nvram restore ready!\n");
    while(nvram_restore_ready_retry < MAX_NVRAM_RESTRORE_READY_RETRY_NUM){
        nvram_restore_ready_retry++;
        property_get("service.nvram_init", nvram_init_val, "");
        if(strcmp(nvram_init_val, "Ready") == 0){
            SLOGD("nvram restore ready!\n");
            break;
        }else{
            usleep(500*1000);
        }
    }

    if(nvram_restore_ready_retry >= MAX_NVRAM_RESTRORE_READY_RETRY_NUM){
        SLOGD("Get nvram restore ready fail!\n");
    }

    Ret = NvramAccessForOMADM(&mStGet, true);
    SLOGD("OMADM NVRAM read  Ret=%d, IsEnable=%d, Usb=%d, Adb=%d, Rndis=%d", Ret, mStGet.iIsEnable, mStGet.iUsb, mStGet.iAdb, mStGet.iRndis);
    if (Ret < 0) {
        SLOGE("vold main read NVRAM failed!");
    } else {
        if (1 == mStGet.iIsEnable) {//usb enable
            //do nothing        
        } else {//usb disable
            char* usb_cmode_path = get_usb_cmode_path();
            if ((fd = open(usb_cmode_path, O_WRONLY)) < 0) {
                SLOGE("Unable to open %s", usb_cmode_path);
                return (void *)(-1);
            }

            count = snprintf(value, sizeof(value), "%d\n", mStGet.iIsEnable);
            Ret = write(fd, value, count);
            close(fd);
            if (Ret < 0) {
                SLOGE("Unable to write %s", usb_cmode_path);
            }
        }
    }
    return (void *)(uintptr_t)Ret;
}

void disable_usb_by_dm(){
    pthread_t t;
    int ret;
    ret = pthread_create(&t, NULL, do_disable_usb_by_dm, NULL);
    if (ret) {
       SLOGE("Cannot create thread to do disable_usb_by_dm()");
       return;
    }
}
#endif

int main() {

    VolumeManager *vm;
    CommandListener *cl;
    NetlinkManager *nm;

    char crypto_state[PROPERTY_VALUE_MAX];
    property_get("ro.crypto.state", crypto_state, "");
    if(!strcmp(crypto_state, "unencrypted")) {
        disable_usb_by_dm();
    }

    SLOGI("Vold 2.1 (the revenge) firing up");

    mkdir("/dev/block/vold", 0755);

    /* For when cryptfs checks and mounts an encrypted filesystem */
    klog_set_level(6);

    /* Create our singleton managers */
    if (!(vm = VolumeManager::Instance())) {
        SLOGE("Unable to create VolumeManager");
        exit(1);
    };

    if (!(nm = NetlinkManager::Instance())) {
        SLOGE("Unable to create NetlinkManager");
        exit(1);
    };


    cl = new CommandListener();
    vm->setBroadcaster((SocketListener *) cl);
    nm->setBroadcaster((SocketListener *) cl);

    if (vm->start()) {
        SLOGE("Unable to start VolumeManager (%s)", strerror(errno));
        exit(1);
    }

    if (process_config(vm)) {
        SLOGE("Error reading configuration (%s)... continuing anyways", strerror(errno));
    }

    if (nm->start()) {
        SLOGE("Unable to start NetlinkManager (%s)", strerror(errno));
        exit(1);
    }

    if (sem_init(&coldboot_sem, 0, 0) == -1) {
        SLOGE("Unable to sem_init (%s)", strerror(errno));
        exit(1);       
    }

    coldboot_sent_uevent_count_only = true;
    coldboot("/sys/block");
    coldboot_sent_uevent_count_only = false;
    coldboot("/sys/block");
//    coldboot("/sys/class/switch");

    SLOGI("Coldboot: try to wait for uevents, timeout 5s");
    struct timespec ts;
    ts.tv_sec=time(NULL)+5;
    ts.tv_nsec=0;   
    if (sem_timedwait(&coldboot_sem, &ts) == -1) {
       SLOGE("Coldboot: fail for sem_timedwait(%s)", strerror(errno));       
    }
    else {
       SLOGI("Coldboot: all uevent has handled");
    }

    /* give the default value to false for property, vold.swap.state */
    property_set("vold.swap.state", "0");

    vm->updateProvideAsecFlag();
#ifdef MTK_2SDCARD_SWAP
	vm->swap2Sdcard();
#else
    vm->setStoragePathProperty();
#endif

    vm->setSharedSdState(Volume::State_Mounted);

    if(is_meta_boot())
    {
       vm->mountallVolumes();
    }  

    get_is_nvram_in_data();

#ifdef MTK_SDCARDFS
    pthread_t thread;
    int ret;
    ret = pthread_create(&thread, NULL, sdcardfs_watch_package_list, NULL);
    if (ret) {
        SLOGE("Cannot create thread to do watch_package_list(), ret=%d",ret);  
    }
#endif

    /*
     * Now that we're up, we can respond to commands
     */
    if (cl->startListener()) {
        SLOGE("Unable to start CommandListener (%s)", strerror(errno));
        exit(1);
    }

    // Eventually we'll become the monitoring thread
    while(1) {
        sleep(1000);
    }

    SLOGI("Vold exiting");
    exit(0);
}

static void do_coldboot(DIR *d, int lvl)
{
    struct dirent *de;
    int dfd, fd;

    dfd = dirfd(d);

    fd = openat(dfd, "uevent", O_WRONLY);
    if(fd >= 0) {
        if (coldboot_sent_uevent_count_only) {
           coldboot_sent_uevent_count++;
        }
        else {
           write(fd, "add\n", 4);
        }
        close(fd);
    }

    while((de = readdir(d))) {
        DIR *d2;

        if (de->d_name[0] == '.')
            continue;

        if (de->d_type != DT_DIR && lvl > 0)
            continue;

        fd = openat(dfd, de->d_name, O_RDONLY | O_DIRECTORY);
        if(fd < 0)
            continue;

        d2 = fdopendir(fd);
        if(d2 == 0)
            close(fd);
        else {
            do_coldboot(d2, lvl + 1);
            closedir(d2);
        }
    }
}

static void coldboot(const char *path)
{
    DIR *d = opendir(path);
    if(d) {
        do_coldboot(d, 0);
        closedir(d);
    }
}

static int process_config(VolumeManager *vm)
{
    char fstab_filename[PROPERTY_VALUE_MAX + sizeof(FSTAB_PREFIX)];
    char propbuf[PROPERTY_VALUE_MAX];
    int i;
    int ret = -1;
    int flags;

    property_get("ro.hardware", propbuf, "");
    snprintf(fstab_filename, sizeof(fstab_filename), FSTAB_PREFIX"%s", propbuf);

    fstab = fs_mgr_read_fstab(fstab_filename);
    if (!fstab) {
        SLOGE("failed to open %s\n", fstab_filename);
        return -1;
    }

    /* Loop through entries looking for ones that vold manages */
    for (i = 0; i < fstab->num_entries; i++) {
        if (fs_mgr_is_voldmanaged(&fstab->recs[i])) {
            DirectVolume *dv = NULL;
            flags = 0;

            /* Set any flags that might be set for this volume */
            if (fs_mgr_is_nonremovable(&fstab->recs[i])) {
                flags |= VOL_NONREMOVABLE;
            }
            if (fs_mgr_is_encryptable(&fstab->recs[i])) {
                flags |= VOL_ENCRYPTABLE;
            }
            /* Only set this flag if there is not an emulated sd card */
            if (fs_mgr_is_noemulatedsd(&fstab->recs[i]) &&
                !strcmp(fstab->recs[i].fs_type, "vfat")) {
                flags |= VOL_PROVIDES_ASEC;
            }

            #if defined(MTK_SHARED_SDCARD) && !defined(MTK_EMMC_SUPPORT)
            if(!strcmp(fstab->recs[i].label, "sdcard")){
                char* modified_label = (char*)malloc(strlen("sdcard1")+1);
                strcpy(modified_label, "sdcard1");
                free(fstab->recs[i].label);
                fstab->recs[i].label = modified_label;
                SLOGI("fstab rec: modify label from 'sdcard' to 'sdcard1' for nand project with MTK_SHARED_SDCARD feature");
            }
            #endif
            
            SLOGI("fstab rec: '%s', '%s', %d, '%s', flag=0x%x",
                   fstab->recs[i].label, fstab->recs[i].mount_point, fstab->recs[i].partnum, fstab->recs[i].blk_device, flags);
            
#ifdef MTK_MULTI_PARTITION_MOUNT_ONLY_SUPPORT
            if(!strcmp(fstab->recs[i].label, "usbotg")){
                SLOGI("fstab ignore usbotg with MTK_MULTI_PARTITION_MOUNT_ONLY_SUPPORT");
                continue;
            }
#endif

            dv = new DirectVolume(vm, &(fstab->recs[i]), flags);

            if (dv->addPath(fstab->recs[i].blk_device)) {
                SLOGE("Failed to add devpath %s to volume %s",
                      fstab->recs[i].blk_device, fstab->recs[i].label);
                goto out_fail;
            }

            #ifdef MTK_SHARED_SDCARD
            /* If it supports MTK_SHARED_SDCARD and the storage is the internal sd, set the state to State_Idle for swap2Sdcard() */
            if(dv->IsEmmcStorage())
                dv->setState(Volume::State_Idle);
            #endif

            vm->addVolume(dv);
        }
    }
    ret = 0;

out_fail:
    return ret;
}

char* get_usb_cmode_path(){
	 char propbuf[PROPERTY_VALUE_MAX];
	 property_get("ro.hardware", propbuf, "");

	 if (strstr(propbuf, "mt6595") || strstr(propbuf, "mt6795")) {
	   return (char*)"/sys/class/udc/musb-hdrc/device/cmode";	
	 }
	 else {
	   return (char*)"/sys/devices/platform/mt_usb/cmode";	
	 }	  
}

char* get_usb_device_path_keyword(){
	 char propbuf[PROPERTY_VALUE_MAX];
	 property_get("ro.hardware", propbuf, "");

	 if (strstr(propbuf, "mt6595")) {
	   return (char*)"musb-mtu3d";	
	 }
	 else {
	   return (char*)"mt_usb";
	 }	  
}

bool is_nvram_in_data = true;
void get_is_nvram_in_data() {
  if (!access("/dev/block/platform/mtk-msdc.0/by-name/nvdata", F_OK)) {
      SLOGI("%s: 'nvdata' partition exists!", __FUNCTION__);
      is_nvram_in_data = false;
  }
}

