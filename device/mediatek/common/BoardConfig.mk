#this is platform common Boardconfig

# Use the non-open-source part, if present
-include vendor/mediatek/common/BoardConfigVendor.mk

# for flavor build base project  assignment
ifeq ($(strip $(MTK_BASE_PROJECT)),)
  MTK_PROJECT_NAME := $(subst full_,,$(TARGET_PRODUCT))
else
  MTK_PROJECT_NAME := $(MTK_BASE_PROJECT)
endif
MTK_PROJECT := $(MTK_PROJECT_NAME)
MTK_PATH_SOURCE := vendor/mediatek/proprietary
MTK_ROOT := vendor/mediatek/proprietary
MTK_PATH_COMMON := vendor/mediatek/proprietary/custom/common
MTK_PATH_CUSTOM := vendor/mediatek/proprietary/custom/$(MTK_PROJECT)
MTK_PATH_CUSTOM_PLATFORM := vendor/mediatek/proprietary/custom/$(shell echo $(MTK_PLATFORM) | tr '[A-Z]' '[a-z]')
TARGET_BOARD_KERNEL_HEADERS := device/mediatek/$(shell echo $(MTK_PLATFORM) | tr '[A-Z]' '[a-z]')/kernel-headers \
                               device/mediatek/common/kernel-headers

MTK_CGEN_CFLAGS += -I$(MTK_PATH_CUSTOM)/cgen/cfgdefault  -I$(MTK_PATH_CUSTOM)/cgen/cfgfileinc -I$(MTK_PATH_CUSTOM)/cgen/inc -I$(MTK_PATH_CUSTOM)/cgen
MTK_CGEN_CFLAGS += -I$(MTK_PATH_CUSTOM_PLATFORM)/cgen/cfgdefault  -I$(MTK_PATH_CUSTOM_PLATFORM)/cgen/cfgfileinc -I$(MTK_PATH_CUSTOM_PLATFORM)/cgen/inc -I$(MTK_PATH_CUSTOM_PLATFORM)/cgen
MTK_CGEN_CFLAGS += -I$(MTK_PATH_COMMON)/cgen/cfgdefault  -I$(MTK_PATH_COMMON)/cgen/cfgfileinc -I$(MTK_PATH_COMMON)/cgen/inc -I$(MTK_PATH_COMMON)/cgen

COMMON_GLOBAL_CFLAGS += $(MTK_CGEN_CFLAGS)
COMMON_GLOBAL_CPPFLAGS += $(MTK_CGEN_CFLAGS)

#MTK_PLATFORM := $(shell echo $(MTK_PROJECT_NAME) | awk -F "_" {'print $$1'})
MTK_PATH_PLATFORM := $(MTK_PATH_SOURCE)/platform/$(shell echo $(MTK_PLATFORM) | tr '[A-Z]' '[a-z]')
MTK_PATH_KERNEL := kernel
GOOGLE_RELEASE_RIL := no
#BUILD_NUMBER := $(shell date +%s)
BUILD_NUMBER := $(shell git log --pretty="%h" -1)
#Enable HWUI by default
USE_OPENGL_RENDERER := true

#SELinux Policy File Configuration
BOARD_SEPOLICY_DIRS := \
        device/mediatek/common/sepolicy

#SELinux: MTK modified
ifeq ($(MTK_INTERNAL),yes)
  BOARD_SEPOLICY_REPLACE := \
    keys.conf 
else
  ifeq ($(MTK_SIGNATURE_CUSTOMIZATION),yes)
    BOARD_SEPOLICY_REPLACE := \
      keys.conf 
  else
    BOARD_SEPOLICY_REPLACE :=
  endif
endif

#SELinux: MTK added
BOARD_SEPOLICY_UNION := \
    app.te \
    device.te \
    domain.te \
    file.te \
    file_contexts \
    fs_use \
    installd.te \
    net.te \
    netd.te \
    te_macros \
    vold.te \
    untrusted_app.te \
    platform_app.te \
    system_app.te \
    zygote.te \
    aal.te \
    aee_core_forwarder.te \
    akmd09911.te \
    akmd8963.te \
    akmd8975.te \
    ami304d.te \
    ist8303.te \
    mc6470d.te \
    qmcX983d.te \
    st480.te \
    af7133e.te \
    af7133ed.te \
    mmc3524xd.te \
    atcid.te \
    atci_service.te \
    audiocmdservice_atci.te \
    batterywarning.te \
    bmm050d.te \
    bmm056d.te \
    boot_logo_updater.te \
    br_app_data_service.te \
    BGW.te \
    ccci_fsd.te \
    ccci_mdinit.te \
    statusd.te \
    flashlessd.te \
    ccci_rpcd.te \
    eemcs_fsd.te \
    eemcs_mdinit.te \
    dhcp6c.te \
    dm_agent_binder.te \
    dualmdlogger.te \
    dumpstate.te \
    em_svr.te \
    enableswap.te \
    disableswap.te \
    factory.te \
    fota1.te \
    fuelgauged.te \
    geomagneticd.te \
    GoogleOtaBinder.te \
    gsm0710muxdmd2.te \
    gsm0710muxd.te \
    guiext-server.te \
    ipod.te \
    matv.te \
    mc6420d.te \
    mdlogger.te \
    mdnsd.te \
    memsicd3416x.te \
    st480d.te \
    memsicd35240.te \
    memsicd.te \
    meta_tst.te \
    mmc_ffu.te \
    mmp.te \
    mnld.te \
    mobile_log_d.te \
    mpud6050.te \
    msensord.te \
    mtk_6620_launcher.te \
    mtk_agpsd.te \
    mtkbt.te \
    muxreport.te \
    netdiag.te \
    nvram_agent_binder.te \
    nvram_daemon.te \
    orientationd.te \
    permission_check.te \
    poad.te \
    pppd_dt.te \
    pppd_via.te \
    pq.te \
    recovery.te \
    resmon.te \
    mtkrild.te \
    mtkrildmd2.te \
    viarild.te \
    s62xd.te \
    sn.te \
    epdg_wod.te \
    ipsec.te \
    terservice.te \
    thermald.te \
    thermal_manager.te \
    thermal.te \
    tiny_mkswap.te \
    tiny_swapon.te \
    vdc.te \
    volte_imcb.te \
    volte_ua.te \
    volte_stack.te \
    wmt_loader.te \
    icusbd.te \
    xlog.te \
    mobicore.te \
    install_recovery.te \
    program_binary.te \
    genfs_contexts

 

BOARD_SEPOLICY_UNION += \
	adbd.te \
	bluetooth.te \
	bootanim.te \
	clatd.te \
	debuggerd.te \
	drmserver.te \
	dhcp.te \
	dnsmasq.te \
	gpsd.te \
	hci_attach.te \
	healthd.te \
	hostapd.te \
	inputflinger.te \
	init.te \
	init_shell.te \
	isolated_app.te \
	keystore.te \
	kernel.te \
	lmkd.te \
	logd.te \
	mediaserver.te \
	mtp.te \
	nfc.te \
	racoon.te \
	radio.te \
	rild.te \
	runas.te \
	sdcardd.te \
	servicemanager.te \
	shared_relro.te \
	shell.te \
	system_app.te \
	system_server.te \
	surfaceflinger.te \
	tee.te \
	ueventd.te \
	uncrypt.te \
	watchdogd.te \
	wpa_supplicant.te \
	wpa.te \
	property.te \
	property_contexts \
	service.te \
	dmlog.te \
	MtkCodecService.te \
	ppl_agent.te \
	pvrsrvctl.te \
	wifi2agps.te \
	dex2oat.te \
	emdlogger.te \
	autokd.te \
	ppp.te \
	launchpppoe.te \
	sbchk.te \
	service_contexts \
	ril-3gddaemon.te \
	usbdongled.te \
	zpppd_gprs.te \
	md_ctrl.te \
	cmddumper.te \
	tunman.te

# Include an expanded selection of fonts
EXTENDED_FONT_FOOTPRINT := true

# ptgen
# Add MTK's MTK_PTGEN_OUT definitions
ifeq (,$(strip $(OUT_DIR)))
  ifeq (,$(strip $(OUT_DIR_COMMON_BASE)))
    MTK_PTGEN_OUT_DIR := $(TOPDIR)out
  else
    MTK_PTGEN_OUT_DIR := $(OUT_DIR_COMMON_BASE)/$(notdir $(PWD))
  endif
else
    MTK_PTGEN_OUT_DIR := $(strip $(OUT_DIR))
endif
MTK_PTGEN_PRODUCT_OUT := $(MTK_PTGEN_OUT_DIR)/target/product/$(MTK_TARGET_PROJECT)
MTK_PTGEN_OUT := $(MTK_PTGEN_OUT_DIR)/target/product/$(MTK_TARGET_PROJECT)/obj/PTGEN
MTK_PTGEN_MK_OUT := $(MTK_PTGEN_OUT_DIR)/target/product/$(MTK_TARGET_PROJECT)/obj/PTGEN
MTK_PTGEN_TMP_OUT := $(MTK_PTGEN_OUT_DIR)/target/product/$(MTK_TARGET_PROJECT)/obj/PTGEN_TMP

#Add MTK's hook
-include device/mediatek/build/build/tools/base_rule_hook.mk







#ckt
BOARD_SEPOLICY_UNION += \
     getsystemtype.te \
