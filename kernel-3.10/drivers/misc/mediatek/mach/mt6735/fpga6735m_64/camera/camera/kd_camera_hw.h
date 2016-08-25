#ifndef _KD_CAMERA_HW_H_
#define _KD_CAMERA_HW_H_
 

#include <mach/mt_gpio.h>

#ifdef MTK_MT6306_SUPPORT
#include <mach/dcl_sim_gpio.h>
#endif

#include <mach/mt_pm_ldo.h>
#ifndef CONFIG_MTK_FPGA
#include "pmic_drv.h"
#endif

#ifdef CONFIG_MTK_FPGA
enum {
    GPIO_CAMERA_CMRST_PIN,
    GPIO_CAMERA_CMPDN_PIN_M_GPIO,
    GPIO_CAMERA_CMPDN_PIN,
    GPIO_CAMERA_CMRST1_PIN,
    GPIO_CAMERA_CMRST1_PIN_M_GPIO,
    GPIO_CAMERA_CMPDN1_PIN,
    GPIO_CAMERA_CMPDN1_PIN_M_GPIO,
    PMIC_APP_MAIN_CAMERA_POWER_A,
    PMIC_APP_MAIN_CAMERA_POWER_D,
    PMIC_APP_MAIN_CAMERA_POWER_AF,
    PMIC_APP_MAIN_CAMERA_POWER_IO,
    PMIC_APP_SUB_CAMERA_POWER_D,
};
#endif
//
//Analog 
#define CAMERA_POWER_VCAM_A         PMIC_APP_MAIN_CAMERA_POWER_A
//Digital 
#define CAMERA_POWER_VCAM_D         PMIC_APP_MAIN_CAMERA_POWER_D
//AF 
#define CAMERA_POWER_VCAM_AF        PMIC_APP_MAIN_CAMERA_POWER_AF
//digital io
#define CAMERA_POWER_VCAM_IO        PMIC_APP_MAIN_CAMERA_POWER_IO
//Digital for Sub
#define SUB_CAMERA_POWER_VCAM_D     PMIC_APP_SUB_CAMERA_POWER_D


//FIXME, should defined in DCT tool 

//Main sensor
#ifdef MTK_MT6306_SUPPORT
    // Common phone's reset pin uses extension GPIO10 of mt6306
    #ifdef CAMERA_CMRST_PIN
    #undef CAMERA_CMRST_PIN
    #endif
    #define CAMERA_CMRST_PIN            GPIO10   
#else
    #define CAMERA_CMRST_PIN            GPIO_CAMERA_CMRST_PIN 
    #define CAMERA_CMRST_PIN_M_GPIO     GPIO_CAMERA_CMRST_PIN_M_GPIO
#endif


#define CAMERA_CMPDN_PIN            GPIO_CAMERA_CMPDN_PIN    
#define CAMERA_CMPDN_PIN_M_GPIO     GPIO_CAMERA_CMPDN_PIN_M_GPIO 
 
//FRONT sensor
#define CAMERA_CMRST1_PIN           GPIO_CAMERA_CMRST1_PIN 
#define CAMERA_CMRST1_PIN_M_GPIO    GPIO_CAMERA_CMRST1_PIN_M_GPIO 

#define CAMERA_CMPDN1_PIN           GPIO_CAMERA_CMPDN1_PIN 
#define CAMERA_CMPDN1_PIN_M_GPIO    GPIO_CAMERA_CMPDN1_PIN_M_GPIO 

// Define I2C Bus Num
#define SUPPORT_I2C_BUS_NUM1        3
#define SUPPORT_I2C_BUS_NUM2        1

#endif 
