/*
 * Copyright (C) 2007 The Android Open Source Project
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

#ifndef FOTA_H_
#define FOTA_H_

#include "mtdutils/mtdutils.h"

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

// fota_common.c

void LOG_INFO(const char *msg, ...);
void LOG_ERROR(const char *msg, ...);
void LOG_HEX(const char *str, const char *p, int len);
void convert_unicode_to_char(const char *src, char *dest);
void convert_char_to_unicode(const char *src, unsigned short *dest);

// fota_dev.c

int find_fota_delta_package(const char *root_path);
int install_fota_delta_package(const char *root_path);
void remove_fota_delta_files(const char *root_path);

int fota_scan_partitions(void);
const MtdPartition *fota_find_partition_by_name(const char *name);
int fota_mount_partition(const MtdPartition *partition, const char *mount_point,
        const char *filesystem, int read_only);
int
fota_partition_info(const MtdPartition *partition,
        size_t *total_size, size_t *erase_size, size_t *write_size);
MtdReadContext *fota_read_partition(const MtdPartition *partition);
MtdReadContext *fota_read_partition2(const MtdPartition *partition);
ssize_t fota_read_data_ex(MtdReadContext *ctx, char *data, size_t size, loff_t offset);
void fota_read_close(MtdReadContext *ctx);
MtdWriteContext *fota_write_partition(const MtdPartition *partition);
ssize_t fota_write_data(MtdWriteContext *ctx, const char *data, size_t len);
int fota_write_block_ex(MtdWriteContext *ctx, const char *data, off_t addr);
int fota_write_data_ex(MtdWriteContext *ctx, const char *data, size_t size, off_t offset);

int fota_write_close(MtdWriteContext *ctx);

#ifdef __cplusplus
}
#endif	/* __cplusplus */


#endif  // FOTA_H_
