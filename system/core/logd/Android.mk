LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE:= logd

LOCAL_SRC_FILES := \
    main.cpp \
    LogCommand.cpp \
    CommandListener.cpp \
    LogListener.cpp \
    LogReader.cpp \
    FlushCommand.cpp \
    LogBuffer.cpp \
    LogBufferElement.cpp \
    LogTimes.cpp \
    LogStatistics.cpp \
    LogWhiteBlackList.cpp \
    libaudit.c \
    LogAudit.cpp \
    event.logtags

LOCAL_SHARED_LIBRARIES := \
    libsysutils \
    liblog \
    libcutils \
    libutils

LOCAL_CFLAGS := -Werror $(shell sed -n 's/^\([0-9]*\)[ \t]*auditd[ \t].*/-DAUDITD_LOG_TAG=\1/p' $(LOCAL_PATH)/event.logtags)

ifeq ($(HAVE_AEE_FEATURE), yes)
    LOCAL_SHARED_LIBRARIES += libaed
    LOCAL_CFALGS += -DHAVE_AEE_FEATURE
    LOCAL_C_INCLUDES += $(MTK_ROOT)/external/aee/binary/inc
endif

include $(BUILD_EXECUTABLE)

include $(call first-makefiles-under,$(LOCAL_PATH))
