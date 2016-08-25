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

#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/wait.h>

#include <linux/kdev_t.h>
#include <linux/fs.h>

#include <cutils/properties.h>

#include <diskconfig/diskconfig.h>

#include <private/android_filesystem_config.h>

#define LOG_TAG "Vold"

#include <cutils/fs.h>
#include <cutils/log.h>

#include <string>

#include "Volume.h"
#include "VolumeManager.h"
#include "ResponseCode.h"
#include "Fat.h"
#include "Process.h"
#include "cryptfs.h"
#include "fat_on_nand.h"
#include "DirectVolume.h"

extern "C" void dos_partition_dec(void const *pp, struct dos_partition *d);
extern "C" void dos_partition_enc(void *pp, struct dos_partition *d);

extern pthread_mutex_t part_rescan_mutex;
extern pthread_cond_t part_rescan_cond;
extern volatile int part_rescan_wait;

/*
 * Media directory - stuff that only media_rw user can see
 */
const char *Volume::MEDIA_DIR           = "/mnt/media_rw";

/*
 * Fuse directory - location where fuse wrapped filesystems go
 */
const char *Volume::FUSE_DIR           = "/storage";

/*
 * Path to external storage where *only* root can access ASEC image files
 */
const char *Volume::SEC_ASECDIR_EXT   = "/mnt/secure/asec";

/*
 * Path to internal storage where *only* root can access ASEC image files
 */
const char *Volume::SEC_ASECDIR_INT   = "/data/app-asec";

/*
 * Path to where secure containers are mounted
 */
const char *Volume::ASECDIR           = "/mnt/asec";

/*
 * Path to where OBBs are mounted
 */
const char *Volume::LOOPDIR           = "/mnt/obb";

const char *Volume::BLKID_PATH = "/system/bin/blkid";

static const char *stateToStr(int state) {
    if (state == Volume::State_Init)
        return "Initializing";
    else if (state == Volume::State_NoMedia)
        return "No-Media";
    else if (state == Volume::State_Idle)
        return "Idle-Unmounted";
    else if (state == Volume::State_Pending)
        return "Pending";
    else if (state == Volume::State_Mounted)
        return "Mounted";
    else if (state == Volume::State_Unmounting)
        return "Unmounting";
    else if (state == Volume::State_Checking)
        return "Checking";
    else if (state == Volume::State_Formatting)
        return "Formatting";
    else if (state == Volume::State_Shared)
        return "Shared-Unmounted";
    else if (state == Volume::State_SharedMnt)
        return "Shared-Mounted";
    else
        return "Unknown-Error";
}

static pid_t sPid = (pid_t)0;

void sigCld(int sig) {
    pid_t pid = (pid_t)0;
    int status;
    SLOGI("===>%s:signal received, %u, sPid = %d", __FUNCTION__, sig, sPid);
    if (sPid > 0) { 
        pid = waitpid(sPid, NULL, 0);
        sPid = 0;
    }

}
Volume::Volume(VolumeManager *vm, const fstab_rec* rec, int flags) {
    mVm = vm;
    mDebug = false;
    mLabel = strdup(rec->label); 
    mUuid = NULL;
    mUserLabel = NULL;
    mState = mPreState = Volume::State_Init;
    mFlags = flags;
    mCurrentlyMountedKdev = -1;
    mPartIdx = rec->partnum;
    mRetryMount = false;
    mIsEmmcStorage = false;
    mIsHwPullOut = false;
#ifdef MTK_MULTI_PARTITION_MOUNT_ONLY_SUPPORT
    mDescription = NULL;
    mOtgNodePath = NULL;
    mPid = -1;
#endif
}

#ifdef MTK_MULTI_PARTITION_MOUNT_ONLY_SUPPORT
Volume::Volume(VolumeManager *vm, const char *label, const char *mount_point, 
                    const char *description, const char *node_path) {
    mVm = vm;
    mDebug = false;
    mLabel = strdup(label);
    mUuid = NULL;
    mUserLabel = NULL;

    mState = mPreState = Volume::State_Init;
    mCurrentlyMountedKdev = -1;
    mPartIdx = -1;
    mRetryMount = false;
    mIsEmmcStorage = false;
    mIsHwPullOut = false;

    mDescription = strdup(description);
    mOtgNodePath = strdup(node_path);
    mPid = -1;
}

#endif
Volume::~Volume() {
    free(mLabel);
    free(mUuid);
    free(mUserLabel);
#ifdef MTK_MULTI_PARTITION_MOUNT_ONLY_SUPPORT    
    if (mDescription) {
        free(mDescription);
        mDescription = NULL;
    }
    if (mOtgNodePath) {
        free(mOtgNodePath);
        mOtgNodePath = NULL;
    }
#endif
}

void Volume::setDebug(bool enable) {
    mDebug = enable;
}

dev_t Volume::getDiskDevice() {
    return MKDEV(0, 0);
};

dev_t Volume::getShareDevice() {
    return getDiskDevice();
}

void Volume::handleVolumeShared() {
}

void Volume::handleVolumeUnshared() {
}

int Volume::handleBlockEvent(NetlinkEvent * /*evt*/) {
    errno = ENOSYS;
    return -1;
}

void Volume::setUuid(const char* uuid) {
    char msg[256];

    if (mUuid) {
        free(mUuid);
    }

    if (uuid) {
        mUuid = strdup(uuid);
        snprintf(msg, sizeof(msg), "%s %s \"%s\"", getLabel(),
                getFuseMountpoint(), mUuid);
    } else {
        mUuid = NULL;
        snprintf(msg, sizeof(msg), "%s %s", getLabel(), getFuseMountpoint());
    }

    mVm->getBroadcaster()->sendBroadcast(ResponseCode::VolumeUuidChange, msg,
            false);
}

void Volume::setUserLabel(const char* userLabel) {
    char msg[256];

    if (mUserLabel) {
        free(mUserLabel);
    }

    if (userLabel) {
        mUserLabel = strdup(userLabel);
        snprintf(msg, sizeof(msg), "%s %s \"%s\"", getLabel(),
                getFuseMountpoint(), mUserLabel);
    } else {
        mUserLabel = NULL;
        snprintf(msg, sizeof(msg), "%s %s", getLabel(), getFuseMountpoint());
    }

    mVm->getBroadcaster()->sendBroadcast(ResponseCode::VolumeUserLabelChange,
            msg, false);
}

void Volume::setState(int state, bool isFat32) {
    char msg[255];
    int oldState = mState;

    if (oldState == state) {
        SLOGW("Duplicate state (%d)\n", state);
        return;
    }

    if ((oldState == Volume::State_Pending) && (state != Volume::State_Idle)) {
        mRetryMount = false;
    }

    mPreState = oldState;
    mState = state;

    SLOGD("Volume %s state changing %d (%s) -> %d (%s), isFat32(%d)", mLabel,
         oldState, stateToStr(oldState), mState, stateToStr(mState), isFat32);
    snprintf(msg, sizeof(msg),
             "Volume %s %s state changed from %d (%s) to %d (%s) %d", getLabel(),
             getFuseMountpoint(), oldState, stateToStr(oldState), mState,
             stateToStr(mState), isFat32);


    mVm->getBroadcaster()->sendBroadcast(ResponseCode::VolumeStateChange,
                                         msg, false);
}

int Volume::createDeviceNode(const char *path, int major, int minor) {
    mode_t mode = 0660 | S_IFBLK;
    dev_t dev = (major << 8) | minor;
    if (mknod(path, mode, dev) < 0) {
        if (errno != EEXIST) {
            return -1;
        }
    }
    return 0;
}

int Volume::formatVol(bool wipe) {

    dev_t deviceNodes[DirectVolume::MAX_PARTITIONS];
    bool isForceFat32 = false;

    if (getState() == Volume::State_NoMedia) {
        errno = ENODEV;
        return -1;
    } else if (getState() != Volume::State_Idle) {
        errno = EBUSY;
        return -1;
    }
#ifdef MTK_SHARED_SDCARD
  if(IsEmmcStorage()){
    SLOGE("It is not allowed to format internal SDCARD with MTK_SHARED_SDCARD enabled");
    errno = -EPERM;
    return errno;
  }
#endif
    if (isMountpointMounted(getMountpoint())) {
        SLOGW("Volume is idle but appears to be mounted - fixing");
        setState(Volume::State_Mounted);
        // mCurrentlyMountedKdev = XXX
        errno = EBUSY;
        return -1;
    }

    SLOGI("mDiskNumParts = %d\n", getDeviceNumParts());
    bool formatEntireDevice = (mPartIdx == -1);
    char devicePath[255];
    dev_t diskNode = getDiskDevice();
    dev_t partNode =
        MKDEV(MAJOR(diskNode),
              MINOR(diskNode) + (formatEntireDevice ? 1 : mPartIdx));

    setState(Volume::State_Formatting);

    int ret = -1;
#ifdef MTK_MULTI_PARTITION_MOUNT_ONLY_SUPPORT
    
    char *otgNodePath = getOtgNodePath();
    if (0 == strcmp(getLabel(), "usbotg")) {
        if (NULL == otgNodePath) {
            SLOGE("usbotg: Volume formatVol otgNodePath is NULL");
            goto err;
        }

        SLOGD("usbotg: Volume formatVol otgNodePath = %s", otgNodePath);

        // Only initialize the MBR if we are formatting the entire device
        if (formatEntireDevice) {
            SLOGI("usbotg: Volume formatVol call initializeMbr().\n");
            if (initializeMbr(otgNodePath)) {
                SLOGE("usbotg: Volume formatVol failed to initialize MBR (%s)", strerror(errno));
                goto err;
            }
            SLOGI("usbotg: Volume formatVol exit initializeMbr().\n");
        }

        if (Fat::format(otgNodePath, 0, wipe, isForceFat32)) {
            SLOGE("usbotg: Volume formatVol Failed to format (%s)", strerror(errno));
            goto err;
        }
    } else {
        // Only initialize the MBR if we are formatting the entire device
        if (formatEntireDevice) {
            sprintf(devicePath, "/dev/block/vold/%d:%d",
                    major(diskNode), minor(diskNode));
            SLOGI("Call initializeMbr().\n");
            if (initializeMbr(devicePath)) {
                SLOGE("Failed to initialize MBR (%s)", strerror(errno));
                goto err;
            }
            SLOGI("Exit initializeMbr().\n");
        }
    
        sprintf(devicePath, "/dev/block/vold/%d:%d",
                major(partNode), minor(partNode));
    
        if (mDebug) {
            SLOGI("Formatting volume %s (%s)", getLabel(), devicePath);
        }
    
#ifdef MTK_ICUSB_SUPPORT
        if(!strcmp(getLabel(), "icusb1") || !strcmp(getLabel(), "icusb2")){ 
            isForceFat32 = true;
        }
#endif 
    
        if (Fat::format(devicePath, 0, wipe, isForceFat32)) {
            SLOGE("Failed to format (%s)", strerror(errno));
            goto err;
        }
    }
#else
    {
        // Only initialize the MBR if we are formatting the entire device
        if (formatEntireDevice) {
            sprintf(devicePath, "/dev/block/vold/%d:%d",
                    major(diskNode), minor(diskNode));
            SLOGI("Call initializeMbr().\n");
            if (initializeMbr(devicePath)) {
                SLOGE("Failed to initialize MBR (%s)", strerror(errno));
                goto err;
            }
        }
    
        sprintf(devicePath, "/dev/block/vold/%d:%d",
                major(partNode), minor(partNode));
    
        if (mDebug) {
            SLOGI("Formatting volume %s (%s)", getLabel(), devicePath);
        }
    
#ifdef MTK_ICUSB_SUPPORT
        if(!strcmp(getLabel(), "icusb1") || !strcmp(getLabel(), "icusb2")){ 
            isForceFat32 = true;
        }
#endif 
    
        if (Fat::format(devicePath, 0, wipe, isForceFat32)) {
            SLOGE("Failed to format (%s)", strerror(errno));
            goto err;
        }
    }
#endif
    ret = 0;

err:
    setState(Volume::State_Idle);
    return ret;
}

bool Volume::isMountpointMounted(const char *path) {
    char device[256];
    char mount_path[256];
    char rest[256];
    FILE *fp;
    char line[1024];

    if (!(fp = fopen("/proc/mounts", "r"))) {
        SLOGE("Error opening /proc/mounts (%s)", strerror(errno));
        return false;
    }

    while(fgets(line, sizeof(line), fp)) {
        line[strlen(line)-1] = '\0';
        sscanf(line, "%255s %255s %255s\n", device, mount_path, rest);
        if (!strcmp(mount_path, path)) {
            fclose(fp);
            return true;
        }
    }

    fclose(fp);
    return false;
}

int Volume::mountVol() {
    dev_t deviceNodes[DirectVolume::MAX_PARTITIONS];
    int n, i, rc = 0, curState;
    char errmsg[255];

    int flags = getFlags();
    
    /* we igore the setting of "VOL_PROVIDES_ASEC". if the storage is primary, then we will handle "Asec"  */
    //bool providesAsec = (flags & VOL_PROVIDES_ASEC) != 0;
    const char* externalStorage = getenv("EXTERNAL_STORAGE");
    bool providesAsec = externalStorage && !strcmp(getFuseMountpoint(), externalStorage);


    // TODO: handle "bind" style mounts, for emulated storage

    char decrypt_state[PROPERTY_VALUE_MAX];
    char crypto_state[PROPERTY_VALUE_MAX];
    char encrypt_progress[PROPERTY_VALUE_MAX];

    if (mPreState != Volume::State_Shared && mVm->isSomeVolumeShared()) {
        SLOGI("Some volume is State_Shared, force to share current volume, %s \n", getLabel());
        return mVm->shareVolume(getLabel(), "ums");
    }

    property_get("vold.decrypt", decrypt_state, "");
    property_get("vold.encrypt_progress", encrypt_progress, "");
#ifdef MTK_MULTI_PARTITION_MOUNT_ONLY_SUPPORT
    char *otgNodePath = getOtgNodePath();
#endif

    /* Don't try to mount the volumes if we have not yet entered the disk password
     * or are in the process of encrypting.
     */
    if ((getState() == Volume::State_NoMedia) ||
        ((!strcmp(decrypt_state, "trigger_restart_min_framework") || encrypt_progress[0]) && providesAsec)) {
        snprintf(errmsg, sizeof(errmsg),
                 "Volume %s %s mount failed - no media",
                 getLabel(), getFuseMountpoint());
        mVm->getBroadcaster()->sendBroadcast(
                                         ResponseCode::VolumeMountFailedNoMedia,
                                         errmsg, false);
        errno = ENODEV;
        return -1;
    } else if (getState() != Volume::State_Idle) {
        errno = EBUSY;
        if (getState() == Volume::State_Pending) {
            mRetryMount = true;
        }
        return -1;
    }

    if (isMountpointMounted(getMountpoint())) {
        SLOGW("Volume is idle but appears to be mounted - fixing");
        setState(Volume::State_Mounted);
        // mCurrentlyMountedKdev = XXX
        return 0;
    }

#ifdef MTK_SHARED_SDCARD
    SLOGI("mountvol: IsEmmcStorage=%d", IsEmmcStorage());
    if (IsEmmcStorage()) {
         property_set("ctl.start", "sdcard");
         waitForServiceState("sdcard", "running");
         setState(Volume::State_Mounted);
         return 0;
    }     
#endif

    n = getDeviceNodes((dev_t *) &deviceNodes, DirectVolume::MAX_PARTITIONS);
    if (!n) {
        SLOGE("Failed to get device nodes (%s)\n", strerror(errno));
        return -1;
    }
    SLOGD("Found %d device nodes", n);

#ifndef MTK_EMULATOR_SUPPORT

    if (!IsEmmcStorage() && !strncmp(getLabel(), "sdcard", 6)) {
#ifdef MTK_FAT_ON_NAND
       if( -1 == mLoopDeviceIdx){
#endif
          SLOGD("Reinit SD card");
            if (mVm->reinitExternalSD()){
              SLOGE("Fail: reinitExternalSD()");
              /* Card inserted but fail to reinit, there is something wrong with this card */
              errno = EIO;
              return -1;
            }
#ifdef MTK_FAT_ON_NAND
          }
#endif
    }
    
#endif

    /* If we're running encrypted, and the volume is marked as encryptable and nonremovable,
     * and also marked as providing Asec storage, then we need to decrypt
     * that partition, and update the volume object to point to it's new decrypted
     * block device
     */
    property_get("ro.crypto.state", crypto_state, "");
    if (providesAsec &&
        ((flags & (VOL_NONREMOVABLE | VOL_ENCRYPTABLE))==(VOL_NONREMOVABLE | VOL_ENCRYPTABLE)) &&
        !strcmp(crypto_state, "encrypted") && !isDecrypted()) {
       char new_sys_path[MAXPATHLEN];
       char nodepath[256];
       int new_major, new_minor;

       if (n != 1) {
           /* We only expect one device node returned when mounting encryptable volumes */
           SLOGE("Too many device nodes returned when mounting %s\n", getMountpoint());
           return -1;
       }

       if (cryptfs_setup_volume(getLabel(), MAJOR(deviceNodes[0]), MINOR(deviceNodes[0]),
                                new_sys_path, sizeof(new_sys_path),
                                &new_major, &new_minor)) {
           SLOGE("Cannot setup encryption mapping for %s\n", getMountpoint());
           return -1;
       }
#ifdef MTK_MULTI_PARTITION_MOUNT_ONLY_SUPPORT
    if (0 == strcmp(getLabel(), "usbotg")) {
        if (createDeviceNode(otgNodePath, new_major, new_minor)) {
            SLOGE("usbotg: Volume mountVol Error making device node '%s' (%s)", otgNodePath,
                                                       strerror(errno));
        }
        updateDeviceInfo(otgNodePath, new_major, new_minor);
    } else
#endif
       /* We now have the new sysfs path for the decrypted block device, and the
        * majore and minor numbers for it.  So, create the device, update the
        * path to the new sysfs path, and continue.
        */
    {
        snprintf(nodepath,
                 sizeof(nodepath), "/dev/block/vold/%d:%d",
                 new_major, new_minor);
        if (createDeviceNode(nodepath, new_major, new_minor)) {
            SLOGE("Error making device node '%s' (%s)", nodepath,
                                                       strerror(errno));
        }

        // Todo: Either create sys filename from nodepath, or pass in bogus path so
        //       vold ignores state changes on this internal device.
        updateDeviceInfo(nodepath, new_major, new_minor);

        /* Get the device nodes again, because they just changed */
        n = getDeviceNodes((dev_t *) &deviceNodes, DirectVolume::MAX_PARTITIONS);
        if (!n) {
            SLOGE("Failed to get device nodes (%s)\n", strerror(errno));
            return -1;
        }
    }
    }
#ifdef MTK_MULTI_PARTITION_MOUNT_ONLY_SUPPORT
    if (0 == strcmp(getLabel(), "usbotg")) {
        setState(Volume::State_Checking);

        if (0 != mkdir(getMountpoint(), 0700)) {
            SLOGE("usbotg: Volume mountVol (%s) mkdir failed (%s)" , getMountpoint(), strerror(errno));
        }
        else {
            SLOGD("usbotg: Volume mountVol (%s) mkdir success", getMountpoint());
        }
        if (0 != chown(getMountpoint(), AID_MEDIA_RW, AID_MEDIA_RW)) {
            SLOGE("usbotg: Volume mountVol (%s) mkdir failed (%s)" , getMountpoint(), strerror(errno));
        }
        else {
            SLOGD("usbotg: Volume mountVol (%s) mkdir success", getMountpoint());
        }

        if (0 != mkdir(getFuseMountpoint(), 0755)) {
            SLOGE("usbotg: Volume mountVol (%s) mkdir failed (%s)" , getFuseMountpoint(), strerror(errno));
        }
        else {
            SLOGD("usbotg: Volume mountVol (%s) mkdir success", getFuseMountpoint());
        }

        
        int fd;
        int counter = 0;

        while(1) {            
            counter++;
            if( counter == 10)
            {
            	SLOGD("timeout open otgNodePath counter:%d\n",counter);
            	break;
            }
            
            if ((fd = open(otgNodePath, O_RDONLY)) < 0) {
                 SLOGE(" cannot open device '%s' (errno=%d) (%s)", otgNodePath, errno,strerror(errno));
                 usleep(30000);
            } else {
                 SLOGD(" can access %s successfully", otgNodePath);    
                 close(fd);
                 break;
            }
        }

        if (Fat::check(otgNodePath)) {
            SLOGE("usbotg:  %s fat check fail (%s)\n", otgNodePath, strerror(errno));
            char cmd[255] = {0};
            sprintf(cmd, "/system/bin/sh -c \"rm -r %s\"", getFuseMountpoint());
            system(cmd);
            SLOGD("usbotg: Volume unmountVol cmd = %s !!!", cmd);
            sprintf(cmd, "/system/bin/sh -c \"rm -r %s\"", getMountpoint());
            system(cmd);
            SLOGD("usbotg: Volume unmountVol cmd = %s !!!", cmd);
            return -1;            
        }
        
        if (Fat::doMount(otgNodePath, getMountpoint(), false, false, false, 
            AID_MEDIA_RW, AID_MEDIA_RW, 0007, true)) {
            SLOGE("usbotg: Volume mountVol %s Otg failed to mount via VFAT (%s)\n", otgNodePath, strerror(errno));
            char cmd[255] = {0};
            sprintf(cmd, "/system/bin/sh -c \"rm -r %s\"", getFuseMountpoint());
            system(cmd);
            SLOGD("usbotg: Volume unmountVol cmd = %s !!!", cmd);
            sprintf(cmd, "/system/bin/sh -c \"rm -r %s\"", getMountpoint());
            system(cmd);
            SLOGD("usbotg: Volume unmountVol cmd = %s !!!", cmd);
	    setState(Volume::State_Idle);
            return -1;
        }
    
        extractMetadata(otgNodePath);

        //fork sdcard process instead of witch service in init.rc

        struct sigaction act, oact;
        act.sa_handler = sigCld;
        act.sa_flags = 0;
        sigemptyset(&act.sa_mask);

        if (sigaction(SIGCHLD, &act, &oact) <0) {
            SLOGD("Error occured on signal");
            return -1;
        }   
        
        pid_t pid = fork();

        if (pid < 0) {
            SLOGD("Error occured on fork");
            return 0;
        } else if (pid == 0) {
            SLOGD("usbotg: Volume mountVol go execv,pid=%d", pid);
            const char* mountCmd = "/system/bin/sdcard";
            const char* mountpoint = getMountpoint();
            const char* fusemountpoint = getFuseMountpoint();
            const char* argv0[] = {
                        mountCmd,                
                        "-u",                
                        "1023",               
                        "-g",                             
                        "1023", 
                        "-w",
                        "1023",
                        "-d",
                        mountpoint,
                        fusemountpoint,
                        NULL 
                    };
            SLOGD("%s %s %s %s %s %s %s %s %s %s", argv0[0], argv0[1], argv0[2], argv0[3], argv0[4], argv0[5], argv0[6], argv0[7], argv0[8], argv0[9]);

            if (execv(mountCmd,(char* const*)argv0) < 0) {
                ALOGE("execv(%s) failed: %s\n", mountCmd, strerror(errno));
                SLOGD("usbotg: Volume mountVol mounted failed");
                char cmd[255] = {0};
                sprintf(cmd, "/system/bin/sh -c \"rm -r %s\"", getFuseMountpoint());
                system(cmd);
                SLOGD("usbotg: Volume unmountVol cmd = %s !!!", cmd);
                sprintf(cmd, "/system/bin/sh -c \"rm -r %s\"", getMountpoint());
                system(cmd);
                SLOGD("usbotg: Volume unmountVol cmd = %s !!!", cmd);
                exit(127);
            }
            exit(127);
        } else{
            SLOGD("usbotg: Volume mountVol mounted succed,pid=%d",pid);
            setState(Volume::State_Mounted);
            mPid = pid;
            return 0;
        }
    }
#endif  

    for (i = 0; i < n; i++) {
        char devicePath[255];

        sprintf(devicePath, "/dev/block/vold/%d:%d", major(deviceNodes[i]),
                minor(deviceNodes[i]));

        if (deviceNodes[i] == (dev_t)(-1)) {
            SLOGE("Partition '%s' is invalid dev_t. Skip mounting!", devicePath);
            continue;
        }

        SLOGI("%s being considered for volume %s\n", devicePath, getLabel());

        errno = 0;
        if ((getState() == Volume::State_NoMedia) ) {
            SLOGI("NoMedia! skip mounting the storage. Update errno to ENODEV");
            errno = ENODEV;
            return -1;
        } 
        setState(Volume::State_Checking);

        /*
         * If FS check failed, we should move on to next partition
         * instead of returning an error
         */

__CHECK_FAT_AGAIN:
        if (Fat::check(devicePath)) {
            #if defined(MTK_EMMC_SUPPORT) || defined(MTK_FAT_ON_NAND)
                char auto_format_intsd[PROPERTY_VALUE_MAX] ;
                property_get("persist.vold.auto_format_intsd", auto_format_intsd, "1");
                SLOGI("persist.vold.auto_format_intsd=%s", auto_format_intsd);
                
                if (!strcmp(auto_format_intsd, "1")) {
                    #ifdef MTK_EMMC_SUPPORT
                    if (IsEmmcStorage())
                    #endif
                    #ifdef MTK_FAT_ON_NAND
                    if(!strcmp(getLabel(), "sdcard0")) 
                    #endif
                    {
                        SLOGI("** This should be first boot and internal sd is not formatted. Try to format it. (%s)\n", devicePath);
                        if (Fat::format(devicePath, 0, false)) {
                            SLOGE("Failed to format %s(%d)", strerror(errno), errno);
                        }
                        else {
                            SLOGI("%s format successfully\n", devicePath);
                            property_set("persist.vold.auto_format_intsd", "0");
                            goto __CHECK_FAT_AGAIN;
                        }
                    }
                }
            #endif
            SLOGW("%s failed FS checks, move on to next partition", devicePath);
            continue;                                       
        }
        else {
             #ifdef MTK_EMMC_SUPPORT
             if (IsEmmcStorage()) {
                 property_set("persist.vold.auto_format_intsd", "0");
             }
             #endif
             #ifdef MTK_FAT_ON_NAND
             if (!strcmp(getLabel(), "sdcard0")) {
                 property_set("persist.vold.auto_format_intsd", "0");
             }
             #endif 
        }

        errno = 0;
        int gid;
#ifdef MTK_EMMC_DISCARD     
        if (Fat::doMount(devicePath, getMountpoint(), false, false, false,
                AID_MEDIA_RW, AID_MEDIA_RW, 0007, true, IsEmmcStorage())) {
            SLOGE("%s failed to mount via VFAT (%s)\n", devicePath, strerror(errno));
            continue;
        }
#else //MTK_EMMC_DISCARD   
        if (Fat::doMount(devicePath, getMountpoint(), false, false, false,
                AID_MEDIA_RW, AID_MEDIA_RW, 0007, true)) {
            SLOGE("%s failed to mount via VFAT (%s)\n", devicePath, strerror(errno));
            continue;
        }
#endif //MTK_EMMC_DISCARD           

        SLOGI("providesAsec = %d", providesAsec);

        extractMetadata(devicePath);

#ifdef MTK_2SDCARD_SWAP
        char secImgDir[PATH_MAX];  
        sprintf(secImgDir, "%s/.android_secure", getMountpoint());
        SLOGI("Create %s in advance", secImgDir);
        /* Whether primary or secondary storage, create .android folder in advance to prevent from media out of space for Bindmounts fail 
           This case happens in SWAP feature.
        */
        if (access(secImgDir, R_OK | X_OK)) {
          if (errno == ENOENT) {
              if (mkdir(secImgDir, 0777)) {
                  SLOGE("Failed to create %s (%s)", secImgDir, strerror(errno));
              }
          } else {
              SLOGE("Failed to access %s (%s)", secImgDir, strerror(errno));
          }
        }
#endif

        if (providesAsec && mountAsecExternal() != 0) {
            SLOGE("Failed to mount secure area (%s)", strerror(errno));
            umount(getMountpoint());
            setState(Volume::State_Idle);
            return -1;
        }

        char service[64];
        char service_id[64];
        strcpy(service_id, getMountpoint()+strlen(Volume::MEDIA_DIR)+1);
        snprintf(service, 64, "fuse_%s", service_id);
        property_set("ctl.start", service);
        waitForServiceState(service, "running");

        int fd;
        if ((fd = open(devicePath, O_RDONLY)) < 0) {
           SLOGE("Cannot open device '%s' (errno=%d)", devicePath, errno);
           setState(Volume::State_Mounted);
        }
        else {
           setState(Volume::State_Mounted, Fat::isFat32(fd));
           close(fd);
        }
        mCurrentlyMountedKdev = deviceNodes[i];
        return 0;
    }

    SLOGE("Volume %s found no suitable devices for mounting :(\n", getLabel());

    curState = getState();
    if (curState == Volume::State_NoMedia) {
        SLOGI("Mount fail caused by NoMedia! Update errno to ENODEV");
        errno = ENODEV;
    } 

    if ((curState != Volume::State_NoMedia) && 
        (curState != Volume::State_Mounted)) {
         setState(Volume::State_Idle);
    }

    if(curState == Volume::State_Mounted) {
        return 0;
    }
    return -1;
}

int Volume::mountAsecExternal() {
    char legacy_path[PATH_MAX];
    char secure_path[PATH_MAX];

    snprintf(legacy_path, PATH_MAX, "%s/android_secure", getMountpoint());
    snprintf(secure_path, PATH_MAX, "%s/.android_secure", getMountpoint());

    // Recover legacy secure path
    if (!access(legacy_path, R_OK | X_OK) && access(secure_path, R_OK | X_OK)) {
        if (rename(legacy_path, secure_path)) {
            SLOGE("Failed to rename legacy asec dir (%s)", strerror(errno));
        }
    }

    if (fs_prepare_dir(secure_path, 0770, AID_MEDIA_RW, AID_MEDIA_RW) != 0) {
        SLOGW("fs_prepare_dir failed: %s", strerror(errno));
        return -1;
    }

    if (mount(secure_path, SEC_ASECDIR_EXT, "", MS_BIND, NULL)) {
        SLOGE("Failed to bind mount points %s -> %s (%s)", secure_path,
                SEC_ASECDIR_EXT, strerror(errno));
        return -1;
    }

    return 0;
}

int Volume::doUnmount(const char *path, bool force) {
    int retries = 3;
    int need_to_wait_count = 0;
    bool need_to_wait = false;

    bool isHotPlug = mVm->getHotPlug();
    if (isHotPlug == true) {
        retries = 5;        
    }

    SLOGD("doUnmount: %s retries = %d, isHotPlug=%d", path, retries, isHotPlug);

    if (mDebug) {
        SLOGD("Unmounting {%s}, force = %d", path, force);
    }

    while (retries--) {
        if (!umount(path) || errno == EINVAL || errno == ENOENT) {
            SLOGI("%s sucessfully unmounted", path);
            return 0;
        }

        int action = 0;

        if (force) {
            if (retries == 1) {
                action = 2; // SIGKILL
            } else if (retries == 2) {
                action = 1; // SIGHUP
            }
        }

        SLOGW("Failed to unmount %s (%s, retries %d, action %d)",
                path, strerror(errno), retries, action);

        Process::killProcessesWithOpenFiles(path, action);
        if (retries > 0) 
          usleep(1000*1000);
        
        if(isHotPlug && (retries == 1))
            usleep(1000*1000);

    }
    errno = EBUSY;
    SLOGE("Giving up on unmount %s (%s)", path, strerror(errno));
    Process::FindProcessesWithOpenFiles(path);
    return -1;
}

int Volume::unmountVol(bool force, bool revert) {
    int i, rc;

    int flags = getFlags();
    bool providesAsec = (flags & VOL_PROVIDES_ASEC) != 0;
    bool is_emulated_sd = false;
    #ifdef MTK_SHARED_SDCARD
    if (IsEmmcStorage()) {
         providesAsec = false;
         is_emulated_sd = true;
    }     
    #endif

    if (getState() != Volume::State_Mounted) {
        SLOGE("Volume %s unmount request when not mounted", getLabel());
        errno = EINVAL;
        return UNMOUNT_NOT_MOUNTED_ERR;
    }

#ifdef MTK_MULTI_PARTITION_MOUNT_ONLY_SUPPORT
    if (false == isMountpointMounted(getFuseMountpoint())) {
        SLOGD("usbotg: Volume unmountVol %s is already unmounted", getFuseMountpoint());
        goto unmount_success;
    }
#endif
    setState(Volume::State_Unmounting);
    usleep(1000 * 100); // Give the framework some time to react

    char service[64];
    char service_id[64];
    strcpy(service_id, getMountpoint()+strlen(Volume::MEDIA_DIR)+1);
    snprintf(service, 64, "fuse_%s", service_id);
    if (is_emulated_sd) {
       snprintf(service, 64, "sdcard");
    }    

#ifdef MTK_MULTI_PARTITION_MOUNT_ONLY_SUPPORT
    if (0 == strcmp(getLabel(), "usbotg")) {
        sPid = mPid;
        SLOGD("usbotg: Volume unmountVol ,mPid=%d",mPid);
        if(-1 != mPid)
        {
            kill(mPid,SIGKILL);
        }

        SLOGD("usbotg: Volume unmountVol ,mountpoint=%s",getFuseMountpoint());
        if (doUnmount(getFuseMountpoint(), force)) {
            SLOGE("Failed to unmount %s (%s)", getFuseMountpoint(), strerror(errno));
            goto fail_remount_secure;
        }
        if (doUnmount(getMountpoint(), force)) {
            SLOGE("Failed to unmount %s (%s)", getMountpoint(), strerror(errno));
            goto fail_remount_secure;
        }
        SLOGI("usbotg: Volume unmounted sucessfully");

    } else {
        property_set("ctl.stop", service);
        /* Give it a chance to stop.  I wish we had a synchronous way to determine this... */
        //sleep(1);
        waitForServiceState(service, "stopped");



        // TODO: determine failure mode if FUSE times out

        if (providesAsec && doUnmount(Volume::SEC_ASECDIR_EXT, force) != 0) {
            SLOGE("Failed to unmount secure area on %s (%s)", getMountpoint(), strerror(errno));
            goto out_mounted;
        }

        /* Now that the fuse daemon is dead, unmount it */
        if (doUnmount(getFuseMountpoint(), force) != 0) {
        SLOGE("Failed to unmount %s (%s)", getFuseMountpoint(), strerror(errno));
            goto fail_remount_secure;
        }

        /* Unmount the real sd card */
        if (!is_emulated_sd) {
            if (doUnmount(getMountpoint(), force) != 0) {
                SLOGE("Failed to unmount %s (%s)", getMountpoint(), strerror(errno));
                goto fail_remount_secure;
            }

            SLOGI("%s unmounted successfully", getMountpoint());
        }
    }
#else
    {
        #define MAX_NEED_TO_WAIT 10
        bool need_to_wait = false;
        int need_to_wait_count = 0;

        do {
          Process::killProcessesWithOpenFiles(getFuseMountpoint(), 0, "mdlogger", &need_to_wait);
          if(need_to_wait) {
              need_to_wait_count++;
              SLOGI("need_to_wait=%d, need_to_wait_count=%d,", need_to_wait, need_to_wait_count);
              usleep(1000*1000);
          }
          else {
            break;
          } 
        } while (need_to_wait && need_to_wait_count < MAX_NEED_TO_WAIT);

        property_set("ctl.stop", service);
        /* Give it a chance to stop.  I wish we had a synchronous way to determine this... */
        //sleep(1);
        waitForServiceState(service, "stopped");



        // TODO: determine failure mode if FUSE times out

        if (providesAsec && doUnmount(Volume::SEC_ASECDIR_EXT, force) != 0) {
            SLOGE("Failed to unmount secure area on %s (%s)", getMountpoint(), strerror(errno));
            goto out_mounted;
        }

        /* Now that the fuse daemon is dead, unmount it */
        if (doUnmount(getFuseMountpoint(), force) != 0) {
            SLOGE("Failed to unmount %s (%s)", getFuseMountpoint(), strerror(errno));
            goto fail_remount_secure;
        }

        /* Unmount the real sd card */
        if (!is_emulated_sd) {
            if (doUnmount(getMountpoint(), force) != 0) {
                SLOGE("Failed to unmount %s (%s)", getMountpoint(), strerror(errno));
                goto fail_remount_secure;
            }

            SLOGI("%s unmounted successfully", getMountpoint());
        }
    }
#endif
    /* If this is an encrypted volume, and we've been asked to undo
     * the crypto mapping, then revert the dm-crypt mapping, and revert
     * the device info to the original values.
     */
    if (revert && isDecrypted()) {
        cryptfs_revert_volume(getLabel());
        revertDeviceInfo();
        SLOGI("Encrypted volume %s reverted successfully", getMountpoint());
    }

    setUuid(NULL);
    setUserLabel(NULL);
    setState(Volume::State_Idle);
unmount_success:
    mCurrentlyMountedKdev = -1;
#ifdef MTK_MULTI_PARTITION_MOUNT_ONLY_SUPPORT
    if (0 == strcmp(getLabel(), "usbotg")) {
        char cmd[255] = {0};
        sprintf(cmd, "/system/bin/sh -c \"rm -r %s\"", getFuseMountpoint());
        system(cmd);
        SLOGD("usbotg: Volume unmountVol cmd = %s !!!", cmd);
        sprintf(cmd, "/system/bin/sh -c \"rm -r %s\"", getMountpoint());
        system(cmd);
        SLOGD("usbotg: Volume unmountVol cmd = %s !!!", cmd);
        //remove(getMountpoint());
    }
#endif
    return 0;

fail_remount_secure:
    if (providesAsec && mountAsecExternal() != 0) {
        SLOGE("Failed to remount secure area (%s)", strerror(errno));
        goto out_nomedia;
    }

out_mounted:
    property_set("ctl.start", service);
    waitForServiceState(service, "running");
    setState(Volume::State_Mounted);
    return -1;

out_nomedia:
    setState(Volume::State_NoMedia);
    return -1;
}

int Volume::initializeMbr(const char *deviceNode) {
    struct disk_info dinfo;
    int ret = -1;
    struct timeval now;
    struct timespec outtime;

    SLOGI("Enter Volume::initializeMbr()\n");   

    memset(&dinfo, 0, sizeof(dinfo));

    if (!(dinfo.part_lst = (struct part_info *) malloc(MAX_NUM_PARTS * sizeof(struct part_info)))) {
        SLOGE("Failed to malloc prt_lst");
        return -1;
    }

    memset(dinfo.part_lst, 0, MAX_NUM_PARTS * sizeof(struct part_info));
    dinfo.device = strdup(deviceNode);
    dinfo.scheme = PART_SCHEME_MBR;
    dinfo.sect_size = 512;
    dinfo.skip_lba = 2048;
    dinfo.num_lba = 0;
    dinfo.num_parts = 1;

    struct part_info *pinfo = &dinfo.part_lst[0];

    pinfo->name = strdup("android_sdcard");
    pinfo->flags |= PART_ACTIVE_FLAG;
    pinfo->type = PC_PART_TYPE_FAT32;
    pinfo->len_kb = -1;
    SLOGI("Volume::initializeMbr() -- calls apply_disk_config()\n");
    int rc = apply_disk_config(&dinfo, 0);
    SLOGI("Volume::initializeMbr() -- exit apply_disk_config()\n");
    if (rc) {
        SLOGE("Failed to apply disk configuration (%d)", rc);
        goto out;
    }

    gettimeofday(&now, NULL);
    outtime.tv_sec = now.tv_sec + PART_RESCAN_UEVENT_TO;
    outtime.tv_nsec = now.tv_usec * 1000L; 
 
    pthread_mutex_lock(&part_rescan_mutex);
    part_rescan_wait = WAIT_ON_ADD_EVENT;
    if((ret = pthread_cond_timedwait(&part_rescan_cond, &part_rescan_mutex, &outtime))){
      SLOGE("Wait for partition rescan uevent %d seconds timeout. ret:%d, error:%s", PART_RESCAN_UEVENT_TO, ret, strerror(ret));
    }
    SLOGD("%s: return: %d(%s)", __FUNCTION__, ret, strerror(ret));

    part_rescan_wait = WAIT_ON_NO_EVENT;
    pthread_mutex_unlock(&part_rescan_mutex);

 out:
    SLOGI("Exit Volume::initializeMbr() -- free name\n");
    free(pinfo->name);
    SLOGI("Exit Volume::initializeMbr() -- free dinfo.device\n");
    free(dinfo.device);
    SLOGI("Exit Volume::initializeMbr() -- free dinfo.part_lst\n");
    free(dinfo.part_lst);
    SLOGI("Exit initializeMbr().\n");
    return rc;
}

/*
 * Use blkid to extract UUID and label from device, since it handles many
 * obscure edge cases around partition types and formats. Always broadcasts
 * updated metadata values.
 */
int Volume::extractMetadata(const char* devicePath) {
    int res = 0;

#ifdef MTK_EMULATOR_SUPPORT
        ALOGI("For emulator, give the fake uuid and label");
        setUuid("1234-ABCD");
        setUserLabel("NO NAME");
#else
        std::string cmd;
        cmd = BLKID_PATH;
        cmd += " -c /dev/null ";
        cmd += devicePath;

        FILE* fp = popen(cmd.c_str(), "r");
        if (!fp) {
            ALOGE("Failed to run %s: %s", cmd.c_str(), strerror(errno));
            res = -1;
            goto done;
        }

        char line[1024];
        char value[128];
        if (fgets(line, sizeof(line), fp) != NULL) {
            ALOGD("blkid identified as %s", line);

            char* start = strstr(line, "UUID=");
            if (start != NULL && sscanf(start + 5, "\"%127[^\"]\"", value) == 1) {
                setUuid(value);
            } else {
                setUuid(NULL);
            }

            start = strstr(line, "LABEL=");
            if (start != NULL && sscanf(start + 6, "\"%127[^\"]\"", value) == 1) {
                setUserLabel(value);
            } else {
                setUserLabel(NULL);
            }
        } else {
            ALOGW("blkid failed to identify %s", devicePath);
            res = -1;
        }

        pclose(fp);

    done:
        if (res == -1) {
            setUuid(NULL);
            setUserLabel(NULL);
        }
#endif
    return res;
}

int Volume::waitForServiceState(const char *name, const char *state) {
    int i;
    char pname[PROPERTY_VALUE_MAX];
    char curState[PROPERTY_VALUE_MAX];
    bool isGotIt=false;
    snprintf(pname, sizeof(pname), "init.svc.%s", name);

    for(i=0; i<50; i++) {
        property_get(pname, curState, "unknown");
        if(!strcmp(state, curState)) {
           isGotIt = true;
           break;
        }
        usleep(1000 * 100);      
    }
    if(isGotIt) {
       SLOGI("%s: service(%s), state is as expected (%s), retry(%d)", __FUNCTION__, name, state, i);    }
    else {
       SLOGE("%s: service(%s), state is expected to (%s), but (%s)", __FUNCTION__, name, state, curState);
       return -1;
    }

    return 0;
}

bool Volume::isExternalSD(void) {
#ifdef MTK_2SDCARD_SWAP
    return (!strcmp(getLabel(), "sdcard1")) ;
#endif
    return 0;
}
bool Volume::isPhoneStorage(void) {
#ifdef MTK_2SDCARD_SWAP
    return (!strcmp(getLabel(), "sdcard0")) ;
#endif
    return 0;
}

