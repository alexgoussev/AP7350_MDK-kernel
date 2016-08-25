#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "bootloader.h"
#include "common.h"
//#include "cutils/properties.h"
#include "cutils/android_reboot.h"
#include "install.h"
#include "minui/minui.h"
#include "minzip/DirUtil.h"
#include "roots.h"
#include "screen_ui.h"
#include "fota.h"

static const char *FOTA1_LOG_FILE = "/cache/recovery/fota1.log";

static const struct option OPTIONS[] = {
  { "fota_delta_path", required_argument, NULL, 'f' },
  { "reboot_to_recovery", no_argument, NULL, 'r' },
  { NULL, 0, NULL, 0 },
};

int main(int argc, char **argv)
{
    int  ret = INSTALL_ERROR;
    const char *fota_delta_path = "/cache/delta/";
    bool  reboot_to_recovery = 1;

    time_t start = time(NULL);

    int arg;
    while ((arg = getopt_long(argc, argv, "", OPTIONS, NULL)) != -1) {
        switch (arg) {
        case 'f': fota_delta_path = optarg; break;
        case 'r': reboot_to_recovery = true; break;
        case '?':
            fprintf(stdout, "Invalid command argument\n");
            continue;
        }
    }

    // If these fail, there's not really anywhere to complain...
    freopen(FOTA1_LOG_FILE, "a", stdout); setbuf(stdout, NULL);
    freopen(FOTA1_LOG_FILE, "a", stderr); setbuf(stderr, NULL);

    //fprintf(stderr, "Starting recovery on %s", ctime(&start));

    load_volume_table();

//    fota_delta_path = "/cache/delta/";
//    reboot_to_recovery = 1;

    if(fota_delta_path!=NULL)
        fprintf(stdout, "fota_delta_path= %s\n", fota_delta_path);

    if(reboot_to_recovery) 
        fprintf(stdout, "reboot_to_recovery set true\n");
    
#ifdef SUPPORT_SBOOT_UPDATE
    sec_init(false);
#endif

    ////if (INSTALL_SUCCESS != install_fota_delta_package("/data"))  {
    fprintf(stdout, "fota_delta_path : %s\n", fota_delta_path ? fota_delta_path : "NULL");
    if (!fota_delta_path)  {
        return INSTALL_ERROR;
    }

    fprintf(stdout, "before update /recovery and /tee1(if supported)\n");
    ret = install_fota_delta_package(fota_delta_path);
    fprintf(stdout, "finish update /recovery and /tee1(if supported)\n");

    if (INSTALL_SUCCESS != ret)  {
        fprintf(stderr, "Upgrade fail : 0x%X (%d)\n", ret, ret);
        return ret;
    }

    char buf[256];
    sprintf(buf, "%srecovery.delta", fota_delta_path);
    ret = unlink(buf);
    if ((ret == 0) || (ret < 0 && errno == ENOENT))  {
        fprintf(stdout, "delete...%s, ret = %s\n", buf, strerror(errno)); // log
    }
    else  {
        fprintf(stdout, "Can not delete %s", buf);
    }


    char buf2[256];
    sprintf(buf2, "%stee1.delta", fota_delta_path);
    ret = unlink(buf2);
    if ((ret == 0) || (ret < 0 && errno == ENOENT))  {
        fprintf(stdout, "delete...%s, ret = %s\n", buf2, strerror(errno)); // log
    }
    else  {
        fprintf(stdout, "Can not delete %s", buf2);
    }
	
    if (reboot_to_recovery)  {
        fprintf(stdout, "reboot to recovery mode\n");
        struct bootloader_message boot;
        memset(&boot, 0, sizeof(boot));
        strlcpy(boot.command, "boot-recovery", sizeof(boot.command));
        strlcpy(boot.recovery, "recovery\n", sizeof(boot.recovery));
        strlcat(boot.recovery, "--fota_delta_path=", sizeof(boot.recovery));
        strlcat(boot.recovery, fota_delta_path, sizeof(boot.recovery));
        set_bootloader_message(&boot);
        sync();

        //Disable reboot in fota1, let DM trigger reboot process
        //android_reboot(ANDROID_RB_RESTART, 0, 0);
    }

    //if (argc == 3)  {
    //    struct bootloader_message boot;
    //    memset(&boot, 0, sizeof(boot));
    //    strlcpy(boot.command, "boot-recovery", sizeof(boot.command));
    //    strlcpy(boot.recovery, "recovery\n", sizeof(boot.recovery));
    //    strlcat(boot.recovery, "--fota_delta_path=", sizeof(boot.recovery));
    //    strlcat(boot.recovery, argv[2], sizeof(boot.recovery));
    //    set_bootloader_message(&boot);
    //    sync();
    //}

    return 0;
}

void write_all_log(void)
{
    return;
}
