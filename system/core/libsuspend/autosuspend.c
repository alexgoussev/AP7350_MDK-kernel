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

#include <stdbool.h>

#define LOG_TAG "libsuspend"
#include <cutils/log.h>

#include <suspend/autosuspend.h>

#include "autosuspend_ops.h"

#if 0 //eric workaround: can't find it?
#include "aee.h"

#include <sys/ioctl.h>
#include <stdio.h>
#include <fcntl.h>
#endif

static struct autosuspend_ops *autosuspend_ops;
static bool autosuspend_enabled;
static bool autosuspend_inited;

static int autosuspend_init(void)
{
    if (autosuspend_inited) {
        return 0;
    }

    autosuspend_ops = autosuspend_earlysuspend_init();
    if (autosuspend_ops) {
        goto out;
    }

/* Remove autosleep so userspace can manager suspend/resume and keep stats */
#if 0
    autosuspend_ops = autosuspend_autosleep_init();
    if (autosuspend_ops) {
        goto out;
    }
#endif

    autosuspend_ops = autosuspend_wakeup_count_init();
    if (autosuspend_ops) {
        goto out;
    }

    if (!autosuspend_ops) {
        ALOGE("failed to initialize autosuspend\n");
        return -1;
    }

out:
    autosuspend_inited = true;

    ALOGV("autosuspend initialized\n");
    return 0;
}

int autosuspend_enable(void)
{
    int ret;

    ret = autosuspend_init();
    if (ret) {
        return ret;
    }

    ALOGV("autosuspend_enable\n");

    if (autosuspend_enabled) {
        return 0;
    }

    ret = autosuspend_ops->enable();
    if (ret) {
        return ret;
    }

    autosuspend_enabled = true;
    return 0;
}

int autosuspend_disable(void)
{
    int ret;

    ret = autosuspend_init();
    if (ret) {
        return ret;
    }

    ALOGV("autosuspend_disable\n");

    if (!autosuspend_enabled) {
        return 0;
    }

    ret = autosuspend_ops->disable();
    if (ret) {
        return ret;
    }

    autosuspend_enabled = false;
    return 0;
}

#if 0 //eric workaround
// AED Exported Functions
static int aee_ioctl_wdt_kick(int value)
{
    int ret = 0;
    int fd = open(AE_WDT_POWERKEY_DEVICE_PATH, O_RDONLY);
    if (fd < 0)	{
        ALOGD("autosuspend:ERROR: open %s failed.\n", AE_WDT_DEVICE_PATH);
        return 1;
    } else {
        //ALOGD("autosuspend:AEEIOCTL_WDT_Kick setIOCTL,value=%x",value);
        if (ioctl(fd, AEEIOCTL_WDT_KICK_POWERKEY, (int)value) != 0) {
            ALOGD("autosuspend :ERROR: aee wdt kick powerkey ioctl failed.\n");
            close (fd);
            return 1;
        }
    }
    close (fd);
    return ret;	
}
#endif

void autosuspend_bl_notify(void)
{
    //aee_ioctl_wdt_kick(WDT_SETBY_PM);
}