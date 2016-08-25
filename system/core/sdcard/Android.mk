LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

ifeq ($(strip $(MTK_SHARED_SDCARD)),true)
LOCAL_CFLAGS    += -DMTK_SHARED_SDCARD
endif

LOCAL_SRC_FILES := sdcard.c
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../logwrapper/include
LOCAL_MODULE := sdcard
LOCAL_CFLAGS := -Wall -Wno-unused-parameter -Werror

LOCAL_SHARED_LIBRARIES := libc libcutils
LOCAL_STATIC_LIBRARIES := liblogwrap

include $(BUILD_EXECUTABLE)
