LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := ion.c
LOCAL_MODULE := libion
LOCAL_MODULE_TAGS := optional
LOCAL_SHARED_LIBRARIES := liblog libdl libcutils
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include $(LOCAL_PATH)/kernel-headers
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include $(LOCAL_PATH)/kernel-headers
LOCAL_CFLAGS := -Werror

ifeq ($(TARGET_BUILD_VARIANT),eng)
ifeq ($(MTK_INTERNAL),yes)
LOCAL_CFLAGS += \
    -D_MTK_ENG_
endif
endif

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := ion.c ion_test.c
LOCAL_MODULE := iontest
LOCAL_MODULE_TAGS := optional tests
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include $(LOCAL_PATH)/kernel-headers
LOCAL_SHARED_LIBRARIES := liblog libdl libcutils libc
LOCAL_CFLAGS := -Werror

ifeq ($(TARGET_BUILD_VARIANT),eng)
ifeq ($(MTK_INTERNAL),yes)
LOCAL_CFLAGS += \
    -D_MTK_ENG_
endif
endif

include $(BUILD_EXECUTABLE)

include $(call all-makefiles-under,$(LOCAL_PATH))
