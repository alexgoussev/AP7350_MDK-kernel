/*
* Copyright (C) 2014 MediaTek Inc.
* Modification based on code covered by the mentioned copyright
* and/or permission notice(s).
*/
/*
 * Copyright (C) 2012 The Android Open Source Project
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
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <libgen.h>
#include <time.h>
#include <sys/swap.h>

#include <linux/loop.h>
#include <private/android_filesystem_config.h>
#include <cutils/android_reboot.h>
#include <cutils/partition_utils.h>
#include <cutils/properties.h>
#include <logwrap/logwrap.h>

#include "mincrypt/rsa.h"
#include "mincrypt/sha.h"
#include "mincrypt/sha256.h"

#include "fs_mgr_priv.h"
#include "fs_mgr_priv_verity.h"

#define KEY_LOC_PROP   "ro.crypto.keyfile.userdata"
#define KEY_IN_FOOTER  "footer"

#define E2FSCK_BIN      "/system/bin/e2fsck"
#define F2FS_FSCK_BIN  "/system/bin/fsck.f2fs"
#define MKSWAP_BIN      "/system/bin/mkswap"
#ifdef MTK_FSTAB_FLAGS
#define MAKE_F2FS     "/sbin/mkfs.f2fs"
#define MAKE_EXT4FS    "/system/bin/make_ext4fs"
#define RESIZE_EXT4   "/system/bin/resize_ext4"
#endif
#define PROTECT_1_MNT_POINT "/protect_f"
#define PROTECT_2_MNT_POINT "/protect_s"

#define FSCK_LOG_FILE   "/dev/fscklogs/log"

#define ZRAM_CONF_DEV   "/sys/block/zram0/disksize"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))

/*
 * gettime() - returns the time in seconds of the system's monotonic clock or
 * zero on error.
 */
static time_t gettime(void)
{
    struct timespec ts;
    int ret;

    ret = clock_gettime(CLOCK_MONOTONIC, &ts);
    if (ret < 0) {
        ERROR("clock_gettime(CLOCK_MONOTONIC) failed: %s\n", strerror(errno));
        return 0;
    }

    return ts.tv_sec;
}

static int wait_for_file(const char *filename, int timeout)
{
    struct stat info;
    time_t timeout_time = gettime() + timeout;
    int ret = -1;

    while (gettime() < timeout_time && ((ret = stat(filename, &info)) < 0))
        usleep(10000);

    return ret;
}

static void check_fs(char *blk_device, char *fs_type, char *target)
{
    int status;
    int ret;
    long tmpmnt_flags = MS_NOATIME | MS_NOEXEC | MS_NOSUID;
    char *tmpmnt_opts = "nomblk_io_submit,errors=remount-ro";
    char *e2fsck_argv[] = {
        E2FSCK_BIN,
        "-y",
        blk_device
    };

    /* Check for the types of filesystems we know how to check */
    if (!strcmp(fs_type, "ext2") || !strcmp(fs_type, "ext3") || !strcmp(fs_type, "ext4")) {
        /*
         * First try to mount and unmount the filesystem.  We do this because
         * the kernel is more efficient than e2fsck in running the journal and
         * processing orphaned inodes, and on at least one device with a
         * performance issue in the emmc firmware, it can take e2fsck 2.5 minutes
         * to do what the kernel does in about a second.
         *
         * After mounting and unmounting the filesystem, run e2fsck, and if an
         * error is recorded in the filesystem superblock, e2fsck will do a full
         * check.  Otherwise, it does nothing.  If the kernel cannot mount the
         * filesytsem due to an error, e2fsck is still run to do a full check
         * fix the filesystem.
         */
        ret = mount(blk_device, target, fs_type, tmpmnt_flags, tmpmnt_opts);
        INFO("%s(): mount(%s,%s,%s)=%d\n", __func__, blk_device, target, fs_type, ret);
        if (!ret) {
            umount(target);
        }

        /*
         * Some system images do not have e2fsck for licensing reasons
         * (e.g. recent SDK system images). Detect these and skip the check.
         */
        if (access(E2FSCK_BIN, X_OK)) {
            INFO("Not running %s on %s (executable not in system image)\n",
                 E2FSCK_BIN, blk_device);
        } else {
            INFO("Running %s on %s\n", E2FSCK_BIN, blk_device);

            ret = android_fork_execvp_ext(ARRAY_SIZE(e2fsck_argv), e2fsck_argv,
                                        &status, true, LOG_KLOG | LOG_FILE,
                                        true, FSCK_LOG_FILE);

            if (ret < 0) {
                /* No need to check for error in fork, we can't really handle it now */
                ERROR("Failed trying to run %s\n", E2FSCK_BIN);
            }
        }
    } else if (!strcmp(fs_type, "f2fs")) {
            char *f2fs_fsck_argv[] = {
                    F2FS_FSCK_BIN,
                    "-f",
                    blk_device
            };
        INFO("Running %s -f %s\n", F2FS_FSCK_BIN, blk_device);

        ret = android_fork_execvp_ext(ARRAY_SIZE(f2fs_fsck_argv), f2fs_fsck_argv,
                                      &status, true, LOG_KLOG | LOG_FILE,
                                      true, FSCK_LOG_FILE);
        if (ret < 0) {
            /* No need to check for error in fork, we can't really handle it now */
            ERROR("Failed trying to run %s\n", F2FS_FSCK_BIN);
        }
    }

    return;
}

#ifdef MTK_FSTAB_FLAGS
static void format_fs(char *blk_device, char *fs_type, char *target, long int length)
{
    int status;
    int ret;
    long tmpmnt_flags = MS_NOATIME | MS_NOEXEC | MS_NOSUID;
    char *tmpmnt_opts = "nomblk_io_submit,errors=remount-ro";

    /* Try to mount and then format block device if mount return error */
    ret = mount(blk_device, target, fs_type, tmpmnt_flags, tmpmnt_opts);
    if(!ret) {
        umount(target);
    } else if (errno == EINVAL){
        ERROR("failed to mount because there is no valid superblock, try to format.\n");
        if (strcmp(fs_type, "ext4") == 0) {
            char *len;
            if (asprintf(&len, "%ld", length) <= 0) {
                ERROR("failed to create %s command for %s\n", fs_type, blk_device);
                ret = -1;
                goto err;
            }
            char *make_ext4fs_argv[] = {
                    MAKE_EXT4FS,
                    "-l",
                    len,
                    blk_device,
            };
            INFO("Running %s on %s\n", MAKE_EXT4FS, blk_device);

            ret = android_fork_execvp_ext(ARRAY_SIZE(make_ext4fs_argv), make_ext4fs_argv,
                    &status, true, LOG_NONE,
                    false, NULL);
            free(len);
            if (ret < 0) {
                /* No need to check for error in fork, we can't really handle it now */
                ERROR("Failed trying to run %s\n", MAKE_EXT4FS);
            }
        } else if (strcmp(fs_type, "f2fs") == 0){
            if (length < 0) {
                ERROR("negative length (%ld) not supported on %s\n", length, fs_type);
                ret = -1;
                goto err;
            }
            char *num_sectors;
            if (asprintf(&num_sectors, "%ld", length / 512) <= 0) {
                ERROR("failed to create %s command for %s\n", fs_type, blk_device);
                ret = -1;
                goto err;
            }
            char *make_f2fs_argv[] = {
                    MAKE_F2FS,
                    "-t",
                    "-d1",
                    blk_device,
                    num_sectors
            };
            INFO("Running %s on %s\n", MAKE_F2FS, blk_device);

            ret = android_fork_execvp_ext(ARRAY_SIZE(make_f2fs_argv), make_f2fs_argv,
                    &status, true, LOG_NONE,
                    false, NULL);
            free(num_sectors);
            if (ret < 0) {
                /* No need to check for error in fork, we can't really handle it now */
                ERROR("Failed trying to run %s\n", MAKE_F2FS);
            }
        } else {
            ERROR("File system type:%s not supported now.\n", fs_type);
        }
    } else {
        ERROR("mount fail:errno = %d\n", errno);
    }
err:
    if (ret != 0) {
        ERROR(" make %s failed on %s with %d(%s)\n", fs_type, blk_device, ret, strerror(errno));
    } else {
        ERROR(" make %s success on %s \n", fs_type, blk_device);        
    }
}

static void resize_fs(char *blk_device, char *key_loc)
{
    int status;
    int ret;

    char *resize_ext4_argv[] = {
            RESIZE_EXT4,
            blk_device,
            key_loc
    };
    if(key_loc == NULL) {
        resize_ext4_argv[2] = strdup("dummy");
    }
    INFO("Running %s on %s\n", RESIZE_EXT4, blk_device);

    ret = android_fork_execvp_ext(ARRAY_SIZE(resize_ext4_argv), resize_ext4_argv,
            &status, true, LOG_NONE,
            false, NULL);
    if (ret < 0) {
        /* No need to check for error in fork, we can't really handle it now */
        ERROR("Failed trying to run %s\n", RESIZE_EXT4);
    }
exit:
    ERROR("Resize ext4 return %d\n", ret);
}
#endif

static void remove_trailing_slashes(char *n)
{
    int len;

    len = strlen(n) - 1;
    while ((*(n + len) == '/') && len) {
      *(n + len) = '\0';
      len--;
    }
}

/*
 * Mark the given block device as read-only, using the BLKROSET ioctl.
 * Return 0 on success, and -1 on error.
 */
static void fs_set_blk_ro(const char *blockdev)
{
    int fd;
    int ON = 1;

    fd = open(blockdev, O_RDONLY);
    if (fd < 0) {
        // should never happen
        return;
    }

    ioctl(fd, BLKROSET, &ON);
    close(fd);
}

int execute_cmd(char *cmd_argv[], int argc) {
      int status;
      int ret = 0;

      ret = android_fork_execvp_ext(argc, cmd_argv, &status, true, LOG_KLOG, false, NULL);
      if (ret != 0) {
          /* No need to check for error in fork, we can't really handle it now */
          ERROR("Failed trying to run %s\n", cmd_argv[0]);
          return -1;
      }
      else {
          NOTICE("Execute '%s', status(%d), WEXITSTATUS(%d) \n", cmd_argv[0], status, WEXITSTATUS(status));
          return WEXITSTATUS(ret);
      }
}

/*
 * __mount(): wrapper around the mount() system call which also
 * sets the underlying block device to read-only if the mount is read-only.
 * See "man 2 mount" for return values.
 */
static int __mount(const char *source, const char *target, const struct fstab_rec *rec, int encryptable)
{
    unsigned long mountflags = rec->flags;
    int ret;
    int save_errno;
    bool is_nvram_in_data = true;

    /* We need this because sometimes we have legacy symlinks
     * that are lingering around and need cleaning up.
     */
    struct stat info;
    if (!lstat(target, &info))
        if ((info.st_mode & S_IFMT) == S_IFLNK)
            unlink(target);
    mkdir(target, 0755);

    NOTICE("%s: target='%s, encryptable=%d \n", __FUNCTION__, target, encryptable);

    if (!access("/dev/block/platform/mtk-msdc.0/by-name/nvdata", F_OK)) {
        NOTICE("%s: 'nvdata' partition exists!", __FUNCTION__);
        is_nvram_in_data = false;
    }

    if(encryptable == FS_MGR_MNTALL_DEV_MIGHT_BE_ENCRYPTED && is_nvram_in_data && (!strcmp(target, PROTECT_1_MNT_POINT) || !strcmp(target, PROTECT_2_MNT_POINT))) {
         NOTICE("encryptable is FS_MGR_MNTALL_DEV_MIGHT_BE_ENCRYPTED. Need to mount '%s' as tmpfs\n", target);
         if ((ret = fs_mgr_do_tmpfs_mount((char *)target))) {
             ERROR("Mount '%s' to tmpfs fail. \n", target);
         }
         else {
             NOTICE("Try to copy modem nvram from emmc to the tmpfs of '%s'\n", target);
             char tmp_mnt_point[256];

             snprintf(tmp_mnt_point, sizeof(tmp_mnt_point),  "/mnt%s", target);
             char *mkdir_argv[] = {"/system/bin/mkdir", tmp_mnt_point};
             execute_cmd(mkdir_argv, ARRAY_SIZE(mkdir_argv));

             if (!access(tmp_mnt_point, F_OK) && (ret = mount(source, tmp_mnt_point, rec->fs_type, mountflags, rec->fs_options))) {
                 ERROR("Fail: mount '%s', errno=%d \n", tmp_mnt_point, errno);
             }
             else {
                char *cp_argv[] = {"/system/bin/cp", "-Rp", tmp_mnt_point, "/"};
                execute_cmd(cp_argv, ARRAY_SIZE(cp_argv));
            
                char *umount_argv[] = {"/system/bin/umount", tmp_mnt_point};
                if(!execute_cmd(umount_argv, ARRAY_SIZE(umount_argv))) {
                    char *rm_argv[] = {"/system/bin/rm", "-rf", tmp_mnt_point};
                    execute_cmd(rm_argv, ARRAY_SIZE(rm_argv));
                }                
             }
         }
    }
    else {
        ret = mount(source, target, rec->fs_type, mountflags, rec->fs_options);
        save_errno = errno;
        INFO("%s(source=%s,target=%s,type=%s)=%d\n", __func__, source, target, rec->fs_type, ret);
        if ((ret == 0) && (mountflags & MS_RDONLY) != 0) {
            fs_set_blk_ro(source);
        }
        errno = save_errno;
    }
    return ret;
}

static int fs_match(char *in1, char *in2)
{
    char *n1;
    char *n2;
    int ret;

    n1 = strdup(in1);
    n2 = strdup(in2);

    remove_trailing_slashes(n1);
    remove_trailing_slashes(n2);

    ret = !strcmp(n1, n2);

    free(n1);
    free(n2);

    return ret;
}

static int device_is_debuggable() {
    int ret = -1;
    char value[PROP_VALUE_MAX];
    ret = __system_property_get("ro.debuggable", value);
    if (ret < 0)
        return ret;
    return strcmp(value, "1") ? 0 : 1;
}

static int device_is_secure() {
    int ret = -1;
    char value[PROP_VALUE_MAX];
    ret = __system_property_get("ro.secure", value);
    /* If error, we want to fail secure */
    if (ret < 0)
        return 1;
    return strcmp(value, "0") ? 1 : 0;
}

static int device_is_force_encrypted() {
    int ret = -1;
    char value[PROP_VALUE_MAX];
    ret = __system_property_get("ro.vold.forceencryption", value);
    if (ret < 0)
        return 0;
    return strcmp(value, "1") ? 0 : 1;
}

/*
 * Tries to mount any of the consecutive fstab entries that match
 * the mountpoint of the one given by fstab->recs[start_idx].
 *
 * end_idx: On return, will be the last rec that was looked at.
 * attempted_idx: On return, will indicate which fstab rec
 *     succeeded. In case of failure, it will be the start_idx.
 * Returns
 *   -1 on failure with errno set to match the 1st mount failure.
 *   0 on success.
 */
static int mount_with_alternatives(struct fstab *fstab, int start_idx, int *end_idx, int *attempted_idx, int encryptable)
{
    int i;
    int mount_errno = 0;
    int mounted = 0;

    if (!end_idx || !attempted_idx || start_idx >= fstab->num_entries) {
      errno = EINVAL;
      if (end_idx) *end_idx = start_idx;
      if (attempted_idx) *end_idx = start_idx;
      return -1;
    }

    /* Hunt down an fstab entry for the same mount point that might succeed */
    for (i = start_idx;
         /* We required that fstab entries for the same mountpoint be consecutive */
         i < fstab->num_entries && !strcmp(fstab->recs[start_idx].mount_point, fstab->recs[i].mount_point);
         i++) {
            /*
             * Don't try to mount/encrypt the same mount point again.
             * Deal with alternate entries for the same point which are required to be all following
             * each other.
             */
            if (mounted) {
                ERROR("%s(): skipping fstab dup mountpoint=%s rec[%d].fs_type=%s already mounted as %s.\n", __func__,
                     fstab->recs[i].mount_point, i, fstab->recs[i].fs_type, fstab->recs[*attempted_idx].fs_type);
                continue;
            }
#ifdef MTK_FSTAB_FLAGS
            if((!(fstab->recs[i].fs_mgr_flags & MF_FORCECRYPT)) && (fstab->recs[i].fs_mgr_flags & MF_AUTOFORMAT)) {
                format_fs(fstab->recs[i].blk_device, fstab->recs[i].fs_type,
                       fstab->recs[i].mount_point, fstab->recs[i].length);
            }

            if(fstab->recs[i].fs_mgr_flags & MF_RESIZE) {
                resize_fs(fstab->recs[i].blk_device, fstab->recs[i].key_loc);
            }
#endif

            if (fstab->recs[i].fs_mgr_flags & MF_CHECK) {
                check_fs(fstab->recs[i].blk_device, fstab->recs[i].fs_type,
                         fstab->recs[i].mount_point);
            }
            if (!__mount(fstab->recs[i].blk_device, fstab->recs[i].mount_point, &fstab->recs[i], encryptable)) {
                *attempted_idx = i;
                mounted = 1;
                if (i != start_idx) {
                    ERROR("%s(): Mounted %s on %s with fs_type=%s instead of %s\n", __func__,
                         fstab->recs[i].blk_device, fstab->recs[i].mount_point, fstab->recs[i].fs_type,
                         fstab->recs[start_idx].fs_type);
                }
            } else {
                /* back up errno for crypto decisions */
                mount_errno = errno;
            }
    }

    /* Adjust i for the case where it was still withing the recs[] */
    if (i < fstab->num_entries) --i;

    *end_idx = i;
    if (!mounted) {
        *attempted_idx = start_idx;
        errno = mount_errno;
        return -1;
    }
    return 0;
}
#ifdef MTK_UBIFS_SUPPORT
//#if 0
#define MAX_MTD_PARTITIONS 40

static struct {
    char name[16];
    int number;
} mtd_part_map[MAX_MTD_PARTITIONS];

static int mtd_part_count = -1;

static void find_mtd_partitions(void)
{
    int fd;
    char buf[1024];
    char *pmtdbufp;
    ssize_t pmtdsize;
    int r;

    fd = open("/proc/mtd", O_RDONLY);
    if (fd < 0)
        return;

    buf[sizeof(buf) - 1] = '\0';
    pmtdsize = read(fd, buf, sizeof(buf) - 1);
    pmtdbufp = buf;
    while (pmtdsize > 0) {
        int mtdnum, mtdsize, mtderasesize;
        char mtdname[16];
        mtdname[0] = '\0';
        mtdnum = -1;
        r = sscanf(pmtdbufp, "mtd%d: %x %x %15s",
                   &mtdnum, &mtdsize, &mtderasesize, mtdname);
        if ((r == 4) && (mtdname[0] == '"')) {
            char *x = strchr(mtdname + 1, '"');
            if (x) {
                *x = 0;
            }
            INFO("mtd partition %d, %s\n", mtdnum, mtdname + 1);
            if (mtd_part_count < MAX_MTD_PARTITIONS) {
                strcpy(mtd_part_map[mtd_part_count].name, mtdname + 1);
                mtd_part_map[mtd_part_count].number = mtdnum;
                mtd_part_count++;
            } else {
                ERROR("too many mtd partitions\n");
            }
        }
        while (pmtdsize > 0 && *pmtdbufp != '\n') {
            pmtdbufp++;
            pmtdsize--;
        }
        if (pmtdsize > 0) {
            pmtdbufp++;
            pmtdsize--;
        }
    }
    close(fd);
}

static int mtd_name_to_number(const char *name)
{
    int n;
    if (mtd_part_count < 0) {
        mtd_part_count = 0;
        find_mtd_partitions();
    }
    for (n = 0; n < mtd_part_count; n++) {
        if (!strcmp(name, mtd_part_map[n].name)) {
            return mtd_part_map[n].number;
        }
    }
    return -1;
}

#define UBI_CTRL_DEV "/dev/ubi_ctrl"
#define UBI_SYS_PATH "/sys/class/ubi"
static int ubi_dev_read_int(int dev, const char *file, int def)
{
    int fd, val = def;
    char path[128], buf[64];

    sprintf(path, UBI_SYS_PATH "/ubi%d/%s", dev, file);
    wait_for_file(path, 5);
    fd = open(path, O_RDONLY);
    if (fd == -1) {
        return val;
    }

    if (read(fd, buf, 64) > 0) {
        val = atoi(buf);
    }

    close(fd);
    return val;
}

// Should include kernel header include/mtd/ubi-user.h
#include <linux/types.h>
#include <asm/ioctl.h>
#define UBI_CTRL_IOC_MAGIC 'o'
#define UBI_IOC_MAGIC 'o'
#define UBI_VOL_NUM_AUTO (-1)
#define UBI_DEV_NUM_AUTO (-1)
#define UBI_IOCATT _IOW(UBI_CTRL_IOC_MAGIC, 64, struct ubi_attach_req)
#define UBI_IOCDET _IOW(UBI_CTRL_IOC_MAGIC, 65, __s32)
#define UBI_IOCMKVOL _IOW(UBI_IOC_MAGIC, 0, struct ubi_mkvol_req)
#define UBI_MAX_VOLUME_NAME 127
#define UBI_VID_OFFSET_AUTO (0)
struct ubi_attach_req {
	__s32 ubi_num;
	__s32 mtd_num;
	__s32 vid_hdr_offset;
	__s8 padding[12];
};

struct ubi_mkvol_req {
	__s32 vol_id;
	__s32 alignment;
	__s64 bytes;
	__s8 vol_type;
	__s8 padding1;
	__s16 name_len;
	__s8 padding2[4];
	char name[UBI_MAX_VOLUME_NAME + 1];
} __packed;

enum {
	UBI_DYNAMIC_VOLUME = 3,
	UBI_STATIC_VOLUME  = 4,
};

// Should include kernel header include/mtd/ubi-user.h

static int ubi_attach_mtd(const char *name)
{
    int ret;
    int mtd_num, ubi_num, vid_off;
    int ubi_ctrl, ubi_dev;
    int vols, avail_lebs, leb_size;
    char path[128];
    struct ubi_attach_req attach_req;
    struct ubi_mkvol_req mkvol_req;
    mtd_num = mtd_name_to_number(name);
    if (mtd_num == -1) {
        return -1;
    }

    for (ubi_num = 0; ubi_num < 4; ubi_num++)
    {
      sprintf(path, "/sys/class/ubi/ubi%d/mtd_num", ubi_num);
      ubi_dev = open(path, O_RDONLY);
      if (ubi_dev != -1)
      {
        ret = read(ubi_dev, path, sizeof(path));
        close(ubi_dev);
        if (ret > 0 && mtd_num == atoi(path))
          return ubi_num;
      }
    }

    ubi_ctrl = open(UBI_CTRL_DEV, O_RDONLY);
    if (ubi_ctrl == -1) {
        return -1;
    }

    memset(&attach_req, 0, sizeof(struct ubi_attach_req));
    attach_req.ubi_num = UBI_DEV_NUM_AUTO;
    attach_req.mtd_num = mtd_num;
    attach_req.vid_hdr_offset = UBI_VID_OFFSET_AUTO;  

    ret = ioctl(ubi_ctrl, UBI_IOCATT, &attach_req);
    if (ret == -1) {
        close(ubi_ctrl);
        return -1;
    }

    ubi_num = attach_req.ubi_num;
   vid_off = attach_req.vid_hdr_offset;
    vols = ubi_dev_read_int(ubi_num, "volumes_count", -1);
    if (vols == 0) {
        sprintf(path, "/dev/ubi%d", ubi_num);
	ret = wait_for_file(path, 50);
        ubi_dev = open(path, O_RDONLY);
        if (ubi_dev == -1) {
            close(ubi_ctrl);
            return ubi_num;
        }
       
        avail_lebs = ubi_dev_read_int(ubi_num, "avail_eraseblocks", 0);
        leb_size = ubi_dev_read_int(ubi_num, "eraseblock_size", 0);
        memset(&mkvol_req, 0, sizeof(struct ubi_mkvol_req));
        mkvol_req.vol_id = UBI_VOL_NUM_AUTO;
        mkvol_req.alignment = 1;
        mkvol_req.bytes = (long long)avail_lebs * leb_size;
        mkvol_req.vol_type = UBI_DYNAMIC_VOLUME;
        ret = snprintf(mkvol_req.name, UBI_MAX_VOLUME_NAME + 1, "%s", name);
        mkvol_req.name_len = ret;
        ioctl(ubi_dev, UBI_IOCMKVOL, &mkvol_req);	
        close(ubi_dev);
    }

    close(ubi_ctrl);
    return ubi_num;
}

static int ubi_detach_dev(int dev)
{
    int ret, ubi_ctrl;

    ubi_ctrl = open(UBI_CTRL_DEV, O_RDONLY);
    if (ubi_ctrl == -1) {
        return -1;
    }

    ret = ioctl(ubi_ctrl, UBI_IOCDET, &dev);
    close(ubi_ctrl);
    return ret;
}
#endif

//#ifdef MTK_UBIFS_SUPPORT
//int ubi_attach_mtd(const char *name);
//#endif
/* When multiple fstab records share the same mount_point, it will
 * try to mount each one in turn, and ignore any duplicates after a
 * first successful mount.
 * Returns -1 on error, and  FS_MGR_MNTALL_* otherwise.
 */
int fs_mgr_mount_all(struct fstab *fstab)
{
    int i = 0;
    int encryptable = FS_MGR_MNTALL_DEV_NOT_ENCRYPTED;
    int error_count = 0;
    int mret = -1;
    int mount_errno = 0;
    int attempted_idx = -1;

    if (!fstab) {
        return -1;
    }

    for (i = 0; i < fstab->num_entries; i++) {
        /* Don't mount entries that are managed by vold */
        if (fstab->recs[i].fs_mgr_flags & (MF_VOLDMANAGED | MF_RECOVERYONLY)) {
            continue;
        }

        /* Skip swap and raw partition entries such as boot, recovery, etc */
        if (!strcmp(fstab->recs[i].fs_type, "swap") ||
            !strcmp(fstab->recs[i].fs_type, "emmc") ||
            !strcmp(fstab->recs[i].fs_type, "mtd")) {
            continue;
        }

		ERROR("[xiaolei] : blk device name %s\n", fstab->recs[i].blk_device);		
#ifdef MTK_UBIFS_SUPPORT
	if (!strcmp(fstab->recs[i].fs_type, "ubifs")) {
		char tmp[25];
		int n = ubi_attach_mtd(fstab->recs[i].blk_device + 4);
		if (n < 0) {
			return -1;
		}

		n = sprintf(tmp, "/dev/ubi%d_0", n);
		free(fstab->recs[i].blk_device);
		fstab->recs[i].blk_device = malloc(n+1);
		sprintf(fstab->recs[i].blk_device, "%s", tmp);
		ERROR("debug : ubifs blk_device %s", fstab->recs[i].blk_device);
	} else if (!strcmp(fstab->recs[i].fs_type, "rawfs")) {
		char tmp[25];
		int n = mtd_name_to_number(fstab->recs[i].blk_device + 4);
		if (n < 0) {
			return -1;
		}

		n = sprintf(tmp, "/dev/block/mtdblock%d", n);
		free(fstab->recs[i].blk_device);
		fstab->recs[i].blk_device = malloc(n+1);
		sprintf(fstab->recs[i].blk_device, "%s", tmp);
		ERROR("debug : rawfs blk_device %s", fstab->recs[i].blk_device);
	}
#endif
        if (fstab->recs[i].fs_mgr_flags & MF_WAIT) {
            wait_for_file(fstab->recs[i].blk_device, WAIT_TIMEOUT);
        }

        if ((fstab->recs[i].fs_mgr_flags & MF_VERIFY) && device_is_secure()) {
            int rc = fs_mgr_setup_verity(&fstab->recs[i]);
            if (device_is_debuggable() && rc == FS_MGR_SETUP_VERITY_DISABLED) {
                INFO("Verity disabled");
            } else if (rc != FS_MGR_SETUP_VERITY_SUCCESS) {
                ERROR("Could not set up verified partition, skipping!\n");
                continue;
            }
        }
        int last_idx_inspected;
        mret = mount_with_alternatives(fstab, i, &last_idx_inspected, &attempted_idx, encryptable);
        i = last_idx_inspected;
        mount_errno = errno;

        /* Deal with encryptability. */
        if (!mret) {
            /* If this is encryptable, need to trigger encryption */
            if (   (fstab->recs[attempted_idx].fs_mgr_flags & MF_FORCECRYPT)
                || (device_is_force_encrypted()
                    && fs_mgr_is_encryptable(&fstab->recs[attempted_idx]))) {
                if (umount(fstab->recs[attempted_idx].mount_point) == 0) {
                    if (encryptable == FS_MGR_MNTALL_DEV_NOT_ENCRYPTED) {
                        ERROR("Will try to encrypt %s %s\n", fstab->recs[attempted_idx].mount_point,
                              fstab->recs[attempted_idx].fs_type);
                        encryptable = FS_MGR_MNTALL_DEV_NEEDS_ENCRYPTION;
                    } else {
                        ERROR("Only one encryptable/encrypted partition supported\n");
                        encryptable = FS_MGR_MNTALL_DEV_MIGHT_BE_ENCRYPTED;
                    }
                } else {
                    INFO("Could not umount %s - allow continue unencrypted\n",
                         fstab->recs[attempted_idx].mount_point);
                    continue;
                }
            }
            /* Success!  Go get the next one */
            continue;
        }

        /* mount(2) returned an error, check if it's encryptable and deal with it */
        if (mret && mount_errno != EBUSY && mount_errno != EACCES &&
            fs_mgr_is_encryptable(&fstab->recs[attempted_idx])) {
            if(partition_wiped(fstab->recs[attempted_idx].blk_device)) {
                ERROR("%s(): %s is wiped and %s %s is encryptable. Suggest recovery...\n", __func__,
                      fstab->recs[attempted_idx].blk_device, fstab->recs[attempted_idx].mount_point,
                      fstab->recs[attempted_idx].fs_type);
                encryptable = FS_MGR_MNTALL_DEV_NEEDS_RECOVERY;
                continue;
            } else {
                /* Need to mount a tmpfs at this mountpoint for now, and set
                 * properties that vold will query later for decrypting
                 */
                ERROR("%s(): possibly an encryptable blkdev %s for mount %s type %s )\n", __func__,
                      fstab->recs[attempted_idx].blk_device, fstab->recs[attempted_idx].mount_point,
                      fstab->recs[attempted_idx].fs_type);
                if (fs_mgr_do_tmpfs_mount(fstab->recs[attempted_idx].mount_point) < 0) {
                    ++error_count;
                    continue;
                }
            }
            encryptable = FS_MGR_MNTALL_DEV_MIGHT_BE_ENCRYPTED;
        } else {
            ERROR("Failed to mount an un-encryptable or wiped partition on"
                   "%s at %s options: %s error: %s\n",
                   fstab->recs[attempted_idx].blk_device, fstab->recs[attempted_idx].mount_point,
                   fstab->recs[attempted_idx].fs_options, strerror(mount_errno));
            ++error_count;
            continue;
        }
    }

    if (error_count) {
        return -1;
    } else {
        return encryptable;
    }
}

/* If tmp_mount_point is non-null, mount the filesystem there.  This is for the
 * tmp mount we do to check the user password
 * If multiple fstab entries are to be mounted on "n_name", it will try to mount each one
 * in turn, and stop on 1st success, or no more match.
 */
int fs_mgr_do_mount(struct fstab *fstab, char *n_name, char *n_blk_device,
                    char *tmp_mount_point)
{
    int i = 0;
    int ret = FS_MGR_DOMNT_FAILED;
    int mount_errors = 0;
    int first_mount_errno = 0;
    char *m;

    if (!fstab) {
        return ret;
    }

    for (i = 0; i < fstab->num_entries; i++) {
        if (!fs_match(fstab->recs[i].mount_point, n_name)) {
            continue;
        }

        /* We found our match */
        /* If this swap or a raw partition, report an error */
        if (!strcmp(fstab->recs[i].fs_type, "swap") ||
            !strcmp(fstab->recs[i].fs_type, "emmc") ||
            !strcmp(fstab->recs[i].fs_type, "mtd")) {
            ERROR("Cannot mount filesystem of type %s on %s\n",
                  fstab->recs[i].fs_type, n_blk_device);
            goto out;
        }

        /* First check the filesystem if requested */
        if (fstab->recs[i].fs_mgr_flags & MF_WAIT) {
            wait_for_file(n_blk_device, WAIT_TIMEOUT);
        }
#ifdef MTK_FSTAB_FLAGS
        if(fstab->recs[i].fs_mgr_flags & MF_RESIZE) {
            resize_fs(n_blk_device, fstab->recs[i].key_loc);
        }
#endif

        if (fstab->recs[i].fs_mgr_flags & MF_CHECK) {
            check_fs(n_blk_device, fstab->recs[i].fs_type,
                     fstab->recs[i].mount_point);
        }

        if ((fstab->recs[i].fs_mgr_flags & MF_VERIFY) && device_is_secure()) {
            int rc = fs_mgr_setup_verity(&fstab->recs[i]);
            if (device_is_debuggable() && rc == FS_MGR_SETUP_VERITY_DISABLED) {
                INFO("Verity disabled");
            } else if (rc != FS_MGR_SETUP_VERITY_SUCCESS) {
                ERROR("Could not set up verified partition, skipping!\n");
                continue;
            }
        }

        /* Now mount it where requested */
        if (tmp_mount_point) {
            m = tmp_mount_point;
        } else {
            m = fstab->recs[i].mount_point;
        }
        if (__mount(n_blk_device, m, &fstab->recs[i], FS_MGR_MNTALL_DEV_NOT_ENCRYPTED)) {
            if (!first_mount_errno) first_mount_errno = errno;
            mount_errors++;
            continue;
        } else {
            ret = 0;
            goto out;
        }
    }
    if (mount_errors) {
        ERROR("Cannot mount filesystem on %s at %s. error: %s\n",
            n_blk_device, m, strerror(first_mount_errno));
        if (first_mount_errno == EBUSY) {
            ret = FS_MGR_DOMNT_BUSY;
        } else {
            ret = FS_MGR_DOMNT_FAILED;
        }
    } else {
        /* We didn't find a match, say so and return an error */
        ERROR("Cannot find mount point %s in fstab\n", fstab->recs[i].mount_point);
    }

out:
    return ret;
}

/*
 * mount a tmpfs filesystem at the given point.
 * return 0 on success, non-zero on failure.
 */
int fs_mgr_do_tmpfs_mount(char *n_name)
{
    int ret;

    ret = mount("tmpfs", n_name, "tmpfs",
                MS_NOATIME | MS_NOSUID | MS_NODEV, CRYPTO_TMPFS_OPTIONS);
    if (ret < 0) {
        ERROR("Cannot mount tmpfs filesystem at %s\n", n_name);
        return -1;
    }

    /* Success */
    return 0;
}

int fs_mgr_unmount_all(struct fstab *fstab)
{
    int i = 0;
    int ret = 0;

    if (!fstab) {
        return -1;
    }

    while (fstab->recs[i].blk_device) {
        if (umount(fstab->recs[i].mount_point)) {
            ERROR("Cannot unmount filesystem at %s\n", fstab->recs[i].mount_point);
            ret = -1;
        }
        i++;
    }

    return ret;
}

/* This must be called after mount_all, because the mkswap command needs to be
 * available.
 */
int fs_mgr_swapon_all(struct fstab *fstab)
{
    int i = 0;
    int flags = 0;
    int err = 0;
    int ret = 0;
    int status;
    char *mkswap_argv[2] = {
        MKSWAP_BIN,
        NULL
    };

    if (!fstab) {
        return -1;
    }

    for (i = 0; i < fstab->num_entries; i++) {
        /* Skip non-swap entries */
        if (strcmp(fstab->recs[i].fs_type, "swap")) {
            continue;
        }

        if (fstab->recs[i].zram_size > 0) {
            /* A zram_size was specified, so we need to configure the
             * device.  There is no point in having multiple zram devices
             * on a system (all the memory comes from the same pool) so
             * we can assume the device number is 0.
             */
            FILE *zram_fp;

            zram_fp = fopen(ZRAM_CONF_DEV, "r+");
            if (zram_fp == NULL) {
                ERROR("Unable to open zram conf device %s\n", ZRAM_CONF_DEV);
                ret = -1;
                continue;
            }
            fprintf(zram_fp, "%d\n", fstab->recs[i].zram_size);
            fclose(zram_fp);
        }

        if (fstab->recs[i].fs_mgr_flags & MF_WAIT) {
            wait_for_file(fstab->recs[i].blk_device, WAIT_TIMEOUT);
        }

        /* Initialize the swap area */
        mkswap_argv[1] = fstab->recs[i].blk_device;
        err = android_fork_execvp_ext(ARRAY_SIZE(mkswap_argv), mkswap_argv,
                                      &status, true, LOG_KLOG, false, NULL);
        if (err) {
            ERROR("mkswap failed for %s\n", fstab->recs[i].blk_device);
            ret = -1;
            continue;
        }

        /* If -1, then no priority was specified in fstab, so don't set
         * SWAP_FLAG_PREFER or encode the priority */
        if (fstab->recs[i].swap_prio >= 0) {
            flags = (fstab->recs[i].swap_prio << SWAP_FLAG_PRIO_SHIFT) &
                    SWAP_FLAG_PRIO_MASK;
            flags |= SWAP_FLAG_PREFER;
        } else {
            flags = 0;
        }
        err = swapon(fstab->recs[i].blk_device, flags);
        if (err) {
            ERROR("swapon failed for %s\n", fstab->recs[i].blk_device);
            ret = -1;
        }
    }

    return ret;
}

/*
 * key_loc must be at least PROPERTY_VALUE_MAX bytes long
 *
 * real_blk_device must be at least PROPERTY_VALUE_MAX bytes long
 */
int fs_mgr_get_crypt_info(struct fstab *fstab, char *key_loc, char *real_blk_device, int size)
{
    int i = 0;

    if (!fstab) {
        return -1;
    }
    /* Initialize return values to null strings */
    if (key_loc) {
        *key_loc = '\0';
    }
    if (real_blk_device) {
        *real_blk_device = '\0';
    }

    /* Look for the encryptable partition to find the data */
    for (i = 0; i < fstab->num_entries; i++) {
        /* Don't deal with vold managed enryptable partitions here */
        if (fstab->recs[i].fs_mgr_flags & MF_VOLDMANAGED) {
            continue;
        }
        if (!(fstab->recs[i].fs_mgr_flags & (MF_CRYPT | MF_FORCECRYPT))) {
            continue;
        }

        /* We found a match */
        if (key_loc) {
            strlcpy(key_loc, fstab->recs[i].key_loc, size);
        }
        if (real_blk_device) {
            strlcpy(real_blk_device, fstab->recs[i].blk_device, size);
        }
        break;
    }

    return 0;
}
