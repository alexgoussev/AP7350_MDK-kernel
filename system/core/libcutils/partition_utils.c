/*
* Copyright (C) 2014 MediaTek Inc.
* Modification based on code covered by the mentioned copyright
* and/or permission notice(s).
*/
/*
 * Copyright 2011, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mount.h> /* for BLKGETSIZE */
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cutils/properties.h>

#define LOG_TAG "partition_utils"
#include <log/log.h>
#include <errno.h>
#include <cutils/partition_utils.h>

static int only_one_char(char *buf, int len, char c)
{
    int i, ret;

    ret = 1;
    for (i=0; i<len; i++) {
        if (buf[i] != c) {
            ret = 0;
            break;
        }
    }
    return ret;
}

int partition_wiped(char *source)
{
    char buf[4096];
    int fd, ret;

    if ((fd = open(source, O_RDONLY)) < 0) {
        return 0;
    }

    ret = read(fd, buf, sizeof(buf));
    close(fd);

    if (ret != sizeof(buf)) {
        return 0;
    }

    /* Check for all zeros */
    if (only_one_char(buf, sizeof(buf), 0)) {
       return 1;
    }

    /* Check for all ones */
    if (only_one_char(buf, sizeof(buf), 0xff)) {
       return 1;
    }

    return 0;
}

int misc_set_phone_encrypt_state(const struct phone_encrypt_state *in) {	
#ifdef MTK_EMMC_SUPPORT
    int dev = -1;
    char dev_name[256];
    int count;

    if (!access("/dev/block/platform/mtk-msdc.0/by-name/para", F_OK)) {
       strcpy(dev_name, "/dev/block/platform/mtk-msdc.0/by-name/para");
    }
    else {
       strcpy(dev_name, "/dev/misc");
    }

    dev = open(dev_name, O_WRONLY);
    if (dev < 0)  {
        SLOGE("Can't open %s\n(%s)\n", dev_name, strerror(errno));
        return -1;
    }

    if (lseek(dev, PHONE_ENCRYPT_OFFSET, SEEK_SET) == -1) {
        SLOGE("Failed seeking %s\n(%s)\n", dev_name, strerror(errno));
        close(dev);
        return -1;
    }

    count = write(dev, in, sizeof(*in));
    if (count != sizeof(*in)) {
        SLOGE("Failed writing %s\n(%s)\n", dev_name, strerror(errno));
        return -1;
    }
    if (close(dev) != 0) {
        SLOGE("Failed closing %s\n(%s)\n", dev_name, strerror(errno));
        return -1;
    }
#else
     (void*)in;
#endif
    return 0;
}

int misc_get_phone_encrypt_state(struct phone_encrypt_state *in) {
#ifdef MTK_EMMC_SUPPORT
    int dev = -1;
    char dev_name[256];
    int count;

    if (!access("/dev/block/platform/mtk-msdc.0/by-name/para", F_OK)) {
       strcpy(dev_name, "/dev/block/platform/mtk-msdc.0/by-name/para");
    }
    else {
       strcpy(dev_name, "/dev/misc");
    }	

    dev = open(dev_name, O_RDONLY);
    if (dev < 0)  {
        SLOGE("Can't open %s\n(%s)\n", dev_name, strerror(errno));
        return -1;
    }

    if (lseek(dev, PHONE_ENCRYPT_OFFSET, SEEK_SET) == -1) {
        SLOGE("Failed seeking %s\n(%s)\n", dev_name, strerror(errno));
        close(dev);
        return -1;
    }

    count = read(dev, in, sizeof(*in));
    if (count != sizeof(*in)) {
        SLOGE("Failed reading %s\n(%s)\n", dev_name, strerror(errno));
        return -1;
    }
    if (close(dev) != 0) {
        SLOGE("Failed closing %s\n(%s)\n", dev_name, strerror(errno));
        return -1;
    }
#else
     (void*)in;
#endif
    return 0;

}

