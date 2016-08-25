# Copyright Statement:
#
# This software/firmware and related documentation ("MediaTek Software") are
# protected under relevant copyright laws. The information contained herein
# is confidential and proprietary to MediaTek Inc. and/or its licensors.
# Without the prior written permission of MediaTek inc. and/or its licensors,
# any reproduction, modification, use or disclosure of MediaTek Software,
# and information contained herein, in whole or in part, shall be strictly prohibited.
#
# MediaTek Inc. (C) 2010. All rights reserved.
#
# BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
# THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
# RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
# AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
# NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
# SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
# SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
# THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
# THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
# CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
# SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
# STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
# CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
# AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
# OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
# MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.

ifdef MTK_TARGET_PROJECT
ifneq ($(strip $(MTK_BSP_PACKAGE)),yes)
ifneq ($(strip $(MTK_BASIC_PACKAGE)),yes)

SRC_MTK_API_DIR := vendor/mediatek/proprietary/frameworks/api

.PHONY: checkmtkapi

# eval this to define a rule that runs apicheck.
#
# Args:
#    $(1)  target
#    $(2)  stable api xml file
#    $(3)  api xml file to be tested
#    $(4)  arguments for apicheck
#    $(5)  command to run if apicheck failed

define check-mtk-api
$(TARGET_OUT_COMMON_INTERMEDIATES)/PACKAGING/$(strip $(1))-timestamp: $(2) $(3) $(4) $(APICHECK) $(8)
	@echo "Checking MediaTek API:" $(1)
	$(hide) ( $(APICHECK_COMMAND) $(6) $(2) $(3) $(4) $(5) || ( $(7) ; exit 38 ) )
	$(hide) mkdir -p $$(dir $$@)
	$(hide) touch $$@
checkmtkapi: $(TARGET_OUT_COMMON_INTERMEDIATES)/PACKAGING/$(strip $(1))-timestamp
endef

checkapi: checkmtkapi

# Get MTK SDK API XML of newest level.
last_released_mtk_sdk_version := $(lastword $(call numerically_sort, \
            $(filter-out current, \
                $(patsubst $(SRC_MTK_API_DIR)/%.txt,%, $(wildcard $(SRC_MTK_API_DIR)/*.txt)) \
             )\
        ))

MTK_INTERNAL_PLATFORM_API_FILE := $(TARGET_OUT_COMMON_INTERMEDIATES)/PACKAGING/mediatek_public_api.txt
MTK_INTERNAL_PLATFORM_REMOVED_API_FILE := $(TARGET_OUT_COMMON_INTERMEDIATES)/PACKAGING/mediatek_removed.txt
BUILD_SYSTEM_MTK_EXTENSION := $(TOPDIR)device/mediatek/build/tasks

# Check that the API we're building hasn't broken the last-released SDK version.
# When fails, build will breaks with message from "apicheck_msg_last" text file shown.

$(eval $(call check-mtk-api, \
	checkmtkapi-last, \
	$(SRC_MTK_API_DIR)/$(last_released_mtk_sdk_version).txt, \
	$(MTK_INTERNAL_PLATFORM_API_FILE), \
	$(SRC_MTK_API_DIR)/removed.txt, \
	$(MTK_INTERNAL_PLATFORM_REMOVED_API_FILE), \
	-error 2 -error 3 -error 4 -error 5 -error 6 -error 7 -error 8 -error 9 -error 10 \
	-error 11 -error 12 -error 13 -error 14 -error 15 -error 16 -error 17 -error 18 \
	-error 19 -error 20 -error 21 -error 23 -error 24, \
	cat $(BUILD_SYSTEM_MTK_EXTENSION)/apicheck_msg_last.txt, \
	$(call doc-timestamp-for,mediatek-api-stubs) \
	))


ifeq ($(strip $(MTK_INTERNAL_API_CHECK)), yes)
# Check that the LEGO API list has not been changed (not removed). When fails, build will breaks
# with message from "apicheck_msg_internal" text file shown.
$(eval $(call check-mtk-api, \
	checkmtkinternalapi, \
	$(SRC_MTK_API_DIR)/internal/current.txt, \
	$(MTK_INTERNAL_MONITORING_API_FILE), \
	$(SRC_MTK_API_DIR)/internal/removed.txt, \
	$(SRC_MTK_API_DIR)/internal/removed.txt, \
	-error 2 -error 3 -error 4 -error 5 -error 6 -error 7 -error 8 -error 9 -error 10 -error 11, \
	cat $(BUILD_SYSTEM_MTK_EXTENSION)/apicheck_msg_internal.txt, \
	$(call doc-timestamp-for,mediatek-internal-api-stubs) \
	))

.PHONY: update-mtk-internal-api
update-mtk-internal-api: $(MTK_INTERNAL_MONITORING_API_FILE) | $(ACP)
	@echo "Copying MediaTek's current.txt"
	$(hide) $(ACP) $(MTK_INTERNAL_MONITORING_API_FILE) $(SRC_MTK_API_DIR)/internal/current.txt
endif
endif
endif
endif
