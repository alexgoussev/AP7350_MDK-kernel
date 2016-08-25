ifeq ($(strip $(TARGET_2ND_CPU_VARIANT)),cortex-a53)
include bionic/libc/arch-arm/cortex-a53/cortex-a53.mk
else
include bionic/libc/arch-arm/cortex-a15/cortex-a15.mk
endif
