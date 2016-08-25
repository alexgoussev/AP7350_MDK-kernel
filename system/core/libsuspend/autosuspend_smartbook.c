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

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define LOG_TAG "libsuspend"
#include <cutils/log.h>
#include <suspend/autosuspend.h>

#define SMARTBOOK_SYS_POWER_STATE "/sys/power/sb_state"
#define SMARTBOOK_SYS_SCREEN_STATE "/proc/smb/ScreenComm"

static bool smartbook_mode_enabled;
static bool smartbook_mode_suspended;
static bool smartbook_mode_inited;

static int sSmartBookPowerStatefd;
static int sSmartBookScreenStatefd;

static const char *smartbook_pwr_state_enable = "enable";
static const char *smartbook_pwr_state_disable = "disable";

static const char *smartbook_pwr_state_suspend = "suspend";
static const char *smartbook_pwr_state_resume = "resume";

int smartbook_mode_init(void)
{
    char buf[80] = {0};

    ALOGI("smartbook_mode_init\n");

    if (smartbook_mode_inited) {
        return 0;
    }

    smartbook_mode_inited = true;

    sSmartBookPowerStatefd = open(SMARTBOOK_SYS_POWER_STATE, O_RDWR);

    if (sSmartBookPowerStatefd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGW("Error opening %s: %s\n", SMARTBOOK_SYS_POWER_STATE, buf);
        return -1;
    }

    sSmartBookScreenStatefd = open(SMARTBOOK_SYS_SCREEN_STATE, O_RDWR);

    if (sSmartBookScreenStatefd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGW("Error opening %s: %s\n", SMARTBOOK_SYS_POWER_STATE, buf);
        return -1;
    }

    ALOGI("smartbook_mode_init initialized\n");

    return 0;
}

int smartbook_mode_enable(void)
{
    int ret;
    char buf[80] = {0};

    ret = smartbook_mode_init();
    if (ret < 0) {
        return ret;
    }

    ALOGI("smartbook_mode_enable (enable)...\n");

    if (smartbook_mode_enabled) {
        ALOGI("already enable smartbook_mode\n");
        return 0;
    }

    ret = write(sSmartBookPowerStatefd, smartbook_pwr_state_enable, strlen(smartbook_pwr_state_enable));
    if (ret < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error writing to %s: %s\n", SMARTBOOK_SYS_POWER_STATE, buf);
        goto err;
    }

    ALOGI("smartbook_mode_enable done\n");

    smartbook_mode_enabled = true;
    return 0;

err:
    return ret;
}

int smartbook_mode_disable(void)
{
    int ret;
    char buf[80] = {0};

    ret = smartbook_mode_init();
    if (ret < 0) {
        return ret;
    }

    ALOGI("smartbook_mode_disable (disable)...\n");

    if (!smartbook_mode_enabled) {
        ALOGI("already disable smartbook_mode\n");
        return 0;
    }

    ret = write(sSmartBookPowerStatefd, smartbook_pwr_state_disable, strlen(smartbook_pwr_state_disable));
    if (ret < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error writing to %s: %s\n", SMARTBOOK_SYS_POWER_STATE, buf);
        goto err;
    }

    ALOGI("smartbook_mode_disable done\n");

    smartbook_mode_enabled = false;
    return 0;

err:
    return ret;
}

int smartbook_mode_suspend(void)
{
    int ret;
    char buf[80] = {0};

    ret = smartbook_mode_init();
    if (ret < 0) {
        return ret;
    }

    ALOGI("smartbook_mode_suspend (suspend)...\n");

    if (smartbook_mode_suspended) {
        ALOGI("already suspend smartbook_mode\n");
        return 0;
    }

    ret = write(sSmartBookPowerStatefd, smartbook_pwr_state_suspend, strlen(smartbook_pwr_state_suspend));
    if (ret < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error writing to %s: %s\n", SMARTBOOK_SYS_POWER_STATE, buf);
        goto err;
    }

    ALOGI("smartbook_mode_suspend done\n");

    smartbook_mode_suspended = true;
    return 0;

err:
    return ret;
}

int smartbook_mode_resume(void)
{
    int ret;
    char buf[80] = {0};

    ret = smartbook_mode_init();
    if (ret < 0) {
        return ret;
    }

    ALOGI("smartbook_mode_resume (resume)...\n");

    if (!smartbook_mode_suspended) {
        ALOGI("already resume smartbook_mode\n");
        return 0;
    }

    ret = write(sSmartBookPowerStatefd, smartbook_pwr_state_resume, strlen(smartbook_pwr_state_resume));
    if (ret < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error writing to %s: %s\n", SMARTBOOK_SYS_POWER_STATE, buf);
        goto err;
    }

    ALOGI("smartbook_mode_resume done\n");

    smartbook_mode_suspended = false;
    return 0;

err:
    return ret;
}

int set_smartbook_screen(int cmd, int timeout)
{
    int ret;
    char buf[80] = {0}, cmd_str[128] = {0};

    ret = smartbook_mode_init();
    if (ret < 0) {
        return ret;
    }

    ALOGI("smartbook_screen_control (cmd = %d, timeout = %d)\n", cmd, timeout);

    snprintf(cmd_str, sizeof(cmd_str), "%d %d\n", cmd, timeout);

    ret = write(sSmartBookScreenStatefd, cmd_str, strlen(cmd_str));
    if (ret < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error writing to %s: %s\n", SMARTBOOK_SYS_SCREEN_STATE, buf);
        goto err;
    }

    ALOGI("set_smartbook_screen done\n");

    return 0;

err:
    return ret;
}