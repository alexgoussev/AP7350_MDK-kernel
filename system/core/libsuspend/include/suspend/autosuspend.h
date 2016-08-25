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

#ifndef _LIBSUSPEND_AUTOSUSPEND_H_
#define _LIBSUSPEND_AUTOSUSPEND_H_

#include <sys/cdefs.h>

__BEGIN_DECLS

/*
 * autosuspend_enable
 *
 * Turn on autosuspend in the kernel, allowing it to enter suspend if no
 * wakelocks/wakeup_sources are held.
 *
 *
 *
 * Returns 0 on success, -1 if autosuspend was not enabled.
 */
int autosuspend_enable(void);

/*
 * autosuspend_disable
 *
 * Turn off autosuspend in the kernel, preventing suspend and synchronizing
 * with any in-progress resume.
 *
 * Returns 0 on success, -1 if autosuspend was not disabled.
 */
int autosuspend_disable(void);

/*
 * set_wakeup_callback
 *
 * Set a function to be called each time the device wakes up from suspend.
 */
void set_wakeup_callback(void (*func)(void));

/*
 * autosuspend_bl_notify
 *
 * Returns 0 on success, -1 if autosuspend was not disabled.
 */
void autosuspend_bl_notify(void);

/*
 * [Smart Book] smartbook_mode_enable
 *
 * Turn on smartbook_mode in the kernel
 *
 * Returns 0 on success, -1 if smartbook_mode was not enabled.
 */
int smartbook_mode_enable(void);

/*
 * [Smart Book] smartbook_mode_disable
 *
 * Turn off smartbook_mode in the kernel
 *
 * Returns 0 on success, -1 if smartbook_mode was not disabled.
 */
int smartbook_mode_disable(void);

/*
 * [Smart Book] smartbook_mode_suspend
 *
 * Turn on smartbook_mode suspend in the kernel
 *
 * Returns 0 on success, -1 if smartbook_mode was not suspend.
 */
int smartbook_mode_suspend(void);

/*
 * [Smart Book] smartbook_mode_resume
 *
 * Turn off smartbook_mode suspend in the kernel
 *
 * Returns 0 on success, -1 if smartbook_mode was not resume.
 */
int smartbook_mode_resume(void);

/*
 * [Smart Book] set_smartbook_screen
 *
 * Control smartbook_screen in the kernel
 *
 * Returns 0 on success, -1 if set_smartbook_screen fail.
 */
int set_smartbook_screen(int cmd, int timeout);

__END_DECLS

#endif
