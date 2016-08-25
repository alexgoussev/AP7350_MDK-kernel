/*
* Copyright (C) 2014 MediaTek Inc.
* Modification based on code covered by the mentioned copyright
* and/or permission notice(s).
*/
/*
 *  ion.c
 *
 * Memory Allocator functions for ion
 *
 *   Copyright 2011 Google, Inc
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
#define LOG_TAG "ion"

#include <cutils/log.h>
#include <cutils/properties.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <dlfcn.h>

#include <linux/ion.h>
#include <ion/ion.h>

#include "../../../vendor/mediatek/proprietary/external/udf/ubrd_config.h"     

int ion_open()
{
    int fd = open("/dev/ion", O_RDONLY);
    if (fd < 0)
        ALOGE("open /dev/ion failed!\n");
    return fd;
}

int ion_close(int fd)
{
    int ret = close(fd);
    if (ret < 0) {
        ALOGE("ion close failed!, fd=%d, %d: %s.\n", fd, ret, strerror(errno));
        return -errno;
    }
    return ret;
}

#define LEN 120
static int ion_ioctl(int fd, int req, void *arg)
{
    int ret = ioctl(fd, req, arg);
    if (ret < 0) {
       ALOGE("ioctl %x failed with code fd = %d, %d: %s\n", req, fd, ret, strerror(errno));

       char filename[LEN];
       char buf[LEN];
       snprintf(filename, LEN, "/proc/%ld/fd/%d", (long)getpid(), fd);
       ssize_t num = readlink(filename, buf, LEN);
       if (num < 0) {
          ALOGE("readlink failed fd=%d, %s\n", fd, strerror(errno));
       } else {
          buf[num] = '\0';
          ALOGE("In this %d process fd=%d<->filename=%s.\n", getpid(), fd, buf);
       }
    }
    return ret;
}

int ion_alloc(int fd, size_t len, size_t align, unsigned int heap_mask,
              unsigned int flags, ion_user_handle_t *handle)
{
    int ret;
    struct ion_allocation_data data = {
        .len = len,
        .align = align,
        .heap_id_mask = heap_mask,
        .flags = flags,
    };

    if (handle == NULL) {
        ALOGE("ion_alloc handle is null.\n");
        return -EINVAL;
    }

    ret = ion_ioctl(fd, ION_IOC_ALLOC, &data);
    if (ret < 0)
        return ret;
    *handle = data.handle;
    return ret;
}

int ion_free(int fd, ion_user_handle_t handle)
{
    struct ion_handle_data data = {
        .handle = handle,
    };
    return ion_ioctl(fd, ION_IOC_FREE, &data);
}

int ion_map(int fd, ion_user_handle_t handle, size_t length, int prot,
            int flags, off_t offset, unsigned char **ptr, int *map_fd)
{
    int ret;
    struct ion_fd_data data = {
        .handle = handle,
    };

    if (map_fd == NULL) {
        ALOGE("ion_map map_fd is null.\n");
        return -EINVAL;
    }
    if (ptr == NULL) {
        ALOGE("ion_map ptr is null.\n");
        return -EINVAL;
    }

    ret = ion_ioctl(fd, ION_IOC_MAP, &data);
    if (ret < 0)
        return ret;
    *map_fd = data.fd;
    if (*map_fd < 0) {
        ALOGE("map ioctl returned negative fd\n");
        return -EINVAL;
    }
    *ptr = mmap(NULL, length, prot, flags, *map_fd, offset);
    if (*ptr == MAP_FAILED) {
        ALOGE("mmap failed: %s\n", strerror(errno));
        return -errno;
    }
    return ret;
}

#define DLSYM_FIND_MAX 10
static int dlsym_counter = DLSYM_FIND_MAX;
static void (*fd_bt_rd)(int) = NULL;
int ion_share(int fd, ion_user_handle_t handle, int *share_fd)
{
    int map_fd;
    int ret;
    struct ion_fd_data data = {
        .handle = handle,
    };
    
    if (share_fd == NULL) {
        ALOGE("ion_share share_fd is null fd = %d.\n", fd);
        return -EINVAL;
    }

    ret = ion_ioctl(fd, ION_IOC_SHARE, &data);
    if (ret < 0)
        return ret;
    *share_fd = data.fd;
    if (*share_fd < 0) {
        ALOGE("share ioctl returned negative fd\n");
        return -EINVAL;
    }
    
#ifdef _MTK_ENG_
    if(dlsym_counter > 0 && fd_bt_rd == NULL) {
        dlsym_counter--;
        fd_bt_rd = (void (*)(int))dlsym(RTLD_DEFAULT, "fdleak_record_backtrace");
        if (!fd_bt_rd) {
            ALOGE("[FDLEAK_TEST]dlerror:%s, %d times.\n", dlerror(), (DLSYM_FIND_MAX - dlsym_counter));
        } else {
            ALOGD("[FDLEAK_TEST]fdleak_record_backtrace:%p\n", fd_bt_rd);
        }
    }
    if (fd_bt_rd) {
	fd_bt_rd(*share_fd);
    }
#endif

    return ret;
}

int ion_alloc_fd(int fd, size_t len, size_t align, unsigned int heap_mask,
                 unsigned int flags, int *handle_fd) {
    ion_user_handle_t handle;
    int ret;

    ret = ion_alloc(fd, len, align, heap_mask, flags, &handle);
    if (ret < 0)
        return ret;
    ret = ion_share(fd, handle, handle_fd);
    ion_free(fd, handle);
    return ret;
}

int ion_import(int fd, int share_fd, ion_user_handle_t *handle)
{
    int ret;
    struct ion_fd_data data = {
        .fd = share_fd,
    };

    if (handle == NULL) {
	ALOGE("ion_import handle is null fd = %d, share_fd = %d.\n", fd, share_fd);
        return -EINVAL;
    }

    ret = ion_ioctl(fd, ION_IOC_IMPORT, &data);
    if (ret < 0)
        return ret;
    *handle = data.handle;
    return ret;
}

int ion_sync_fd(int fd, int handle_fd)
{
    struct ion_fd_data data = {
        .fd = handle_fd,
    };
    return ion_ioctl(fd, ION_IOC_SYNC, &data);
}
