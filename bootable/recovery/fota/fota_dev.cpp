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


#ifdef MTK_EMMC_SUPPORT

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mount.h>  // for _IOW, _IOR, mount()
#include <sys/stat.h>
#include <mtd/mtd-user.h>
#undef NDEBUG
#include <assert.h>

#include "mtdutils/mtdutils.h"

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

struct MtdPartition {
    int device_index;
    unsigned int size;
    unsigned int erase_size;
    int type;
    char *name;
};

struct MtdReadContext {
    const MtdPartition *partition;
    char *buffer;
    size_t consumed;
    int fd;
};

struct MtdWriteContext {
    const MtdPartition *partition;
    char *buffer;
    size_t stored;
    int fd;

    off_t* bad_block_offsets;
    int bad_block_alloc;
    int bad_block_count;
};

typedef struct {
    MtdPartition *partitions;
    int partitions_allocd;
    int partition_count;
} MtdState;

static MtdState g_mtd_state = {
    NULL,   // partitions
    0,      // partitions_allocd
    -1      // partition_count
};


#define DUMCHAR_PROC_FILENAME   "/proc/dumchar_info"

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

int fota_scan_partitions(void)
{
    char buf[2048];
    const char *bufp;
    int fd;
    int i;
    ssize_t nbytes;
    int mtdnum;

    if (g_mtd_state.partitions == NULL) {
        const int nump = 32;
        MtdPartition *partitions = (MtdPartition *) malloc(nump * sizeof(*partitions));
        if (partitions == NULL) {
            errno = ENOMEM;
            return -1;
        }
        g_mtd_state.partitions = partitions;
        g_mtd_state.partitions_allocd = nump;
        memset(partitions, 0, nump * sizeof(*partitions));
    }
    g_mtd_state.partition_count = 0;

    /* Initialize all of the entries to make things easier later.
     * (Lets us handle sparsely-numbered partitions, which
     * may not even be possible.)
     */
    for (i = 0; i < g_mtd_state.partitions_allocd; i++) {
        MtdPartition *p = &g_mtd_state.partitions[i];
        if (p->name != NULL) {
            free(p->name);
            p->name = NULL;
        }
        p->device_index = -1;
    }

    /* Open and read the file contents.
     */
    fd = open(DUMCHAR_PROC_FILENAME, O_RDONLY);
    if (fd < 0) {
        goto bail;
    }
    nbytes = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (nbytes < 0) {
        goto bail;
    }
    buf[nbytes] = '\0';

    //printf("buf : %s", buf);

    /* Parse the contents of the file, which looks like:
     *
     *     cat /proc/dumchar_info
     *
     *     Part_Name       Size    StartAddr       Type    MapTo
     *     preloader    0x00040000   0x00000000   1   /dev/mtd/mtd0
     *     dsp_bl       0x000c0000   0x00000001   1   /dev/mtd/mtd1
     *     nvram        0x00300000   0x00000002   1   /dev/mtd/mtd2
     *     seccfg       0x00020000   0x00000003   1   /dev/mtd/mtd3
     *     uboot        0x00060000   0x00000004   1   /dev/mtd/mtd4
     *     bootimg      0x00500000   0x00000005   1   /dev/mtd/mtd5
     *     recovery     0x00500000   0x00000006   1   /dev/mtd/mtd6
     *     sec_ro       0x00120000   0x00000007   1   /dev/mtd/mtd7
     *     misc         0x00060000   0x00000008   1   /dev/mtd/mtd8
     *     logo         0x00300000   0x00000009   1   /dev/mtd/mtd9
     *     expdb        0x000a0000   0x0000000a   1   /dev/mtd/mtd10
     *     android      0x10e00000   0x0000000b   1   /dev/mtd/mtd11
     *     cache        0x03c00000   0x0000000c   1   /dev/mtd/mtd12
     *     usrdata      0x09820000   0x0000000d   1   /dev/mtd/mtd13
     *     bmtpool      0x00000000   0x00000000   1
     */

    mtdnum = 0;
    bufp = buf;
    while (nbytes > 0) {
        int matches;
        char p_name[32], p_size[32], p_addr[32], p_actname[32];
        int p_type = 0;

        p_name[0] = '\0';
        p_size[0] = '\0';
        p_addr[0] = '\0';
        p_actname[0] = '\0';

        matches = sscanf(bufp, "%s %s %s %d %s", p_name, p_size, p_addr, &p_type, p_actname);
        /* This will fail on the first line, which just contains
         * column headers.
         */
        if (matches == 5) {
            //printf("  find : %s\n", p_name);
            MtdPartition *p = &g_mtd_state.partitions[mtdnum];
            p->type = p_type;
            p->device_index = mtdnum;
            p->size = atoh(p_size+2);
            p->erase_size = 0x20000;
            p->name = strdup(p_name);
            if (p->name == NULL) {
                errno = ENOMEM;
                goto bail;
            }
            g_mtd_state.partition_count++;
            mtdnum++;
        }

        /* Eat the line.
         */
        while (nbytes > 0 && *bufp != '\n') {
            bufp++;
            nbytes--;
        }
        if (nbytes > 0) {
            bufp++;
            nbytes--;
        }
    }

    for (i = 0; i < g_mtd_state.partition_count; i++)  {
        printf("  %s %d\n", g_mtd_state.partitions[i].name, g_mtd_state.partitions[i].device_index);
    }

    return g_mtd_state.partition_count;

bail:
    // keep "partitions" around so we can free the names on a rescan.
    g_mtd_state.partition_count = -1;
    return -1;
}

const MtdPartition *fota_find_partition_by_name(const char *name)
{
    if (g_mtd_state.partitions != NULL) {
        int i;
        for (i = 0; i < g_mtd_state.partitions_allocd; i++) {
            MtdPartition *p = &g_mtd_state.partitions[i];
            if (p->device_index >= 0 && p->name != NULL) {
                if (strcmp(p->name, name) == 0) {
                    return p;
                }
            }
        }
    }
    return NULL;
}

int fota_mount_partition(const MtdPartition *partition, const char *mount_point,
        const char *filesystem, int read_only)
{
    return -1;
}

int fota_partition_info(const MtdPartition *partition,
        size_t *total_size, size_t *erase_size, size_t *write_size)
{
    if (total_size != NULL) *total_size = partition->size;
    if (erase_size != NULL) *erase_size = partition->erase_size;
    if (write_size != NULL) *write_size = partition->erase_size;

    return 0;
}

MtdReadContext *fota_read_partition(const MtdPartition *partition)
{
    MtdReadContext *ctx = (MtdReadContext*) malloc(sizeof(MtdReadContext));
    if (ctx == NULL) return NULL;

    ctx->buffer = (char *) malloc(partition->erase_size);
    if (ctx->buffer == NULL) {
        free(ctx);
        return NULL;
    }

    char mtddevname[64];
    sprintf(mtddevname, "/dev/%s", partition->name);
    //printf("fota_read_partition : %s\n", mtddevname);
    ctx->fd = open(mtddevname, O_RDONLY | O_SYNC);
    if (ctx->fd < 0) {
        free(ctx->buffer);
        free(ctx);
        return NULL;
    }

    ctx->partition = partition;
    ctx->consumed = partition->erase_size;
    return ctx;
}

ssize_t fota_read_data_ex(MtdReadContext *ctx, char *data, size_t size, loff_t offset)
{
    loff_t pos = lseek64(ctx->fd, offset, SEEK_SET);
    if (pos != offset) {
        fprintf(stderr, "can not move file pointer 0x%llX 0x%llX\n", pos, offset);
    }

    int r = read(ctx->fd, data, size);
    if (r != (int) size) {
        fprintf(stderr, "can not random read %d %d\n", r, size);
    }

    return r;
}

void fota_read_close(MtdReadContext *ctx)
{
    close(ctx->fd);
    free(ctx->buffer);
    free(ctx);
}

MtdWriteContext *fota_write_partition(const MtdPartition *partition)
{
    MtdWriteContext *ctx = (MtdWriteContext*) malloc(sizeof(MtdWriteContext));
    if (ctx == NULL) return NULL;

    ctx->bad_block_offsets = NULL;
    ctx->bad_block_alloc = 0;
    ctx->bad_block_count = 0;

    ctx->buffer = (char *) malloc(partition->erase_size);
    if (ctx->buffer == NULL) {
        free(ctx);
        return NULL;
    }

    char mtddevname[64];

    sprintf(mtddevname, "/dev/%s", partition->name);
    printf("fota_write_partition open %s\n", mtddevname);
    ctx->fd = open(mtddevname, O_WRONLY | O_SYNC);
    if (ctx->fd < 0) {
        printf("fota_write_partition open %s fail\n", mtddevname);
        free(ctx->buffer);
        free(ctx);
        return NULL;
    }

    ctx->partition = partition;
    ctx->stored = 0;
    return ctx;
}

ssize_t fota_write_data(MtdWriteContext *ctx, const char *data, size_t len)
{
    size_t wrote = 0;
    off_t  off = lseek(ctx->fd, 0, SEEK_CUR);

    wrote = write(ctx->fd, data, len);

    sync();

    off = lseek(ctx->fd, 0, SEEK_CUR);

    return wrote;
}

int fota_write_data_ex(MtdWriteContext *ctx, const char *data, size_t size, off_t offset)
{
    off_t pos = lseek64(ctx->fd, offset, SEEK_SET);
    if (pos != offset)  {
        fprintf(stderr, "can not move file pointer %lX %lX\n", pos, offset);
    }

    int len = write(ctx->fd, data, size);
    if (len != (int) size)  {
        fprintf(stderr, "can not random read %d %d\n", len, size);
    }

    //sync();

    return len;
}

int fota_write_block_ex(MtdWriteContext *ctx, const char *data, off_t addr)
{
    return (int) mtd_write_block_ex(ctx, data, (off64_t)addr);
}

int fota_write_close(MtdWriteContext *ctx)
{
    return mtd_write_close(ctx);
}

#ifdef __cplusplus
}
#endif	/* __cplusplus */

#else  // MTD

#include "mtdutils/mtdutils.h"

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

int fota_scan_partitions(void)
{
    return mtd_scan_partitions();
}

const MtdPartition *fota_find_partition_by_name(const char *name)
{
    return mtd_find_partition_by_name(name);
}

int fota_mount_partition(const MtdPartition *partition, const char *mount_point,
        const char *filesystem, int read_only)
{
    return mtd_mount_partition(partition, mount_point, filesystem, read_only);
}

int fota_partition_info(const MtdPartition *partition,
        size_t *total_size, size_t *erase_size, size_t *write_size)
{
    return mtd_partition_info(partition, total_size, erase_size, write_size);
}

MtdReadContext *fota_read_partition(const MtdPartition *partition)
{
    return mtd_read_partition(partition);
}

ssize_t fota_read_data_ex(MtdReadContext *ctx, char *data, size_t size, loff_t offset)
{
    return mtd_read_data_ex(ctx, data, size, offset);
}

void fota_read_close(MtdReadContext *ctx)
{
    return mtd_read_close(ctx);
}

MtdWriteContext *fota_write_partition(const MtdPartition *partition)
{
    return mtd_write_partition(partition);
}

ssize_t fota_write_data(MtdWriteContext *ctx, const char *data, size_t len)
{
    return mtd_write_data(ctx, data, len);
}

int fota_write_data_ex(MtdWriteContext *ctx, const char *data, size_t size, off_t offset)
{
    return mtd_write_data_ex(ctx, data, size, (off64_t)offset);
}

int fota_write_block_ex(MtdWriteContext *ctx, const char *data, off_t addr)
{
    return mtd_write_block_ex(ctx, data, (off64_t)addr);
}

int fota_write_close(MtdWriteContext *ctx)
{
    return mtd_write_close(ctx);
}

#ifdef __cplusplus
}
#endif	/* __cplusplus */

#endif

