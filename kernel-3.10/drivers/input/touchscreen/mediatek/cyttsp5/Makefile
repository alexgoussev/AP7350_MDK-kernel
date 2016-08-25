include $(srctree)/drivers/misc/mediatek/Makefile.custom

# Linux driver folder
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/$(ARCH_MTK_PROJECT)/touchpanel/cyttsp5/
ccflags-y += -I$(srctree)/arch/arm/mach-$(MTK_PLATFORM)/$(ARCH_MTK_PROJECT)/touchpanel/cyttsp5/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/cyttsp5/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/


obj-y	+=  cyttsp5_core.o
obj-y	+=  cyttsp5_mt_common.o
obj-y	+=  cyttsp5_mtb.o
obj-y	+=  cyttsp5_btn.o
obj-y	+=  cyttsp5_platform.o 
obj-y	+=  mtk_ttda.o
obj-y	+=  cyttsp5_i2c.o
obj-y	+=  cyttsp5_loader.o
obj-y	+=  cyttsp5_device_access.o
