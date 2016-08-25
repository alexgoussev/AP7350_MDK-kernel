LOCAL_PATH:= $(call my-dir)
ifeq ($(MTK_PROJECT), ap7350_65u_l1)
include $(call all-makefiles-under,$(LOCAL_PATH))
endif
