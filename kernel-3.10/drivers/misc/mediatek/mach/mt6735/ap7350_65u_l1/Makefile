
include $(srctree)/drivers/misc/mediatek/Makefile.custom

ifneq ($(CONFIG_ARCH_MTK_PROJECT),)
ARCH_MTK_PROJECT := $(shell echo $(CONFIG_ARCH_MTK_PROJECT))
obj-y   += core/
obj-y   += dct/dct/
endif

ifeq ($(CONFIG_MTK_HDMI_SUPPORT),y)
obj-y	+=  hdmi/
endif

#obj-y   +=  lcm/
obj-y   +=  camera/camera/
obj-$(CONFIG_MTK_VIBRATOR)   +=  vibrator/
obj-$(CONFIG_MTK_LEDS)   +=  leds/
obj-y   +=  keypad/
#obj-y   +=  sound/
obj-y   +=  accdet/

ifeq ($(CONFIG_MTK_SENSOR_SUPPORT),y)
ifeq ($(CONFIG_MTK_CM36652_NEW),y)
obj-y	+=  alsps/cm36652-new/
endif

ifeq ($(CONFIG_MTK_MC3410_NEW),y)
obj-y   +=  accelerometer/mc3410-new/
endif

ifeq ($(CONFIG_MTK_ITG1010_NEW),y)
obj-y   +=  gyroscope/ITG1010-new/
endif

ifeq ($(CONFIG_MTK_AKM09911_NEW),y)
obj-y	+=  magnetometer/akm09911-new/
endif
endif
ifeq ($(CONFIG_MTK_MC3XXX_AUTO),y)
obj-y   +=  accelerometer/mc3xxx_auto/
endif
ifeq ($(CONFIG_KXTJ2_1009_AUTO),y)
obj-y   +=  accelerometer/kxtj2_1009_auto/
endif
ifeq ($(CONFIG_EPL2182_AUTO),y)
obj-y	+=  alsps/EPL2182_auto/
endif
ifeq ($(CONFIG_CKTPS),y)
obj-y	+=  alsps/cktps/
endif
ifeq ($(CONFIG_EPL259X_AUTO),y)
obj-y	+=  alsps/EPL259X_auto/
endif
ifeq ($(CONFIG_ST480_AUTO),y)
obj-y   +=  magnetometer/st480_auto/
endif
ifeq ($(CONFIG_MMC3524X_AUTO),y)
obj-y   +=  magnetometer/mmc3524x_auto/
endif
ifeq ($(CONFIG_MMC3516X_AUTO),y)
obj-y   +=  magnetometer/mmc3516x_auto/
endif
ifeq ($(CONFIG_MTK_AF7133E),y)
obj-y   +=  magnetometer/af7133e/
endif
