#ifndef _CUST_BAT_H_
#define _CUST_BAT_H_

/* stop charging while in talking mode */
#define STOP_CHARGING_IN_TAKLING
#define TALKING_RECHARGE_VOLTAGE 3800
#define TALKING_SYNC_TIME		   60

/* High battery support */
#define HIGH_BATTERY_VOLTAGE_SUPPORT
/*
4.2V电池——
1. 0℃以下，充电电流为零; 
2. 0℃~45℃，充电电流≤0.5C;
3. 45℃以上，充电电流为零

4.35V电池——
1. 0℃以下，不允许充电；
2. 0℃-10℃，充电电流0.1C且≤400mA，充电限制电压4.2V；（此温度范围充电电流/限制电压根据具体项目的电池规格书，注意不同厂家的参数可能不同）
3. 10℃-45℃，充电电流≤0.5C，充电限制电压4.35V；
4. 45℃以上，不允许充电。

整机加在电池两端的充电电压不能超过规格电压，即4.2V电池充电电压不能高于4.2V，4.35V电池充电电压不能高于4.35V。
 59℃以上高温告警提示，-20℃以下低温告警提示
 
提示语中不要出现“电池过热”，过热提示用“手机过热”；——此项针对所有机型
不能出现要求用户“移除电池”的提示；——此项仅针对内置电池的机型
*/
/* Battery Temperature Protection */
#define MTK_TEMPERATURE_RECHARGE_SUPPORT

#ifdef CONFIG_TEMPERATURE_CONTROL_CHARGING
#define MAX_CHARGE_TEMPERATURE  45						//最高限充温度
#define MAX_CHARGE_TEMPERATURE_MINUS_X_DEGREE	42		//最高回充温度
#define MIN_CHARGE_TEMPERATURE  0						//最低限充温度
#define MIN_CHARGE_TEMPERATURE_PLUS_X_DEGREE	6		//最低回充温度
#define MAX_CHARGE_NOTIFY_TEMPERATURE 58				//高温报警温度
#define MIN_CHARGE_NOTIFY_TEMPERATURE -20				//低温报警温度
#define MAX_CHARGE_POWEROFF_TEMPERATURE 60	            //高温自动掉电温度

#ifdef HIGH_BATTERY_VOLTAGE_SUPPORT
#define CURRENT_LIMIT_BOUNDARY_TEMPERATURE 10			//高低温限流温度
#define MAX_CURRENT_LIMIT_BOUNDARY_BATTERY BATTERY_VOLT_04_420000_V		//低温以下限制充电电压
#define MIN_CURRENT_LIMIT_BOUNDARY_BATTERY BATTERY_VOLT_04_350000_V		//低温以下限制充电电压
#define AC_LESS_N_DEGRESS_CHARGER_CURRENT	CHARGE_CURRENT_600_00_MA	//高低温之低温限流 0.1C ~ 400mA
#define AC_LESS_N_NORMAL_CHARGER_CURRENT	CHARGE_CURRENT_1050_00_MA	//高低温之高温限流 < 0.5C
#else
#define CURRENT_LIMIT_BOUNDARY_TEMPERATURE 0
#define MAX_CURRENT_LIMIT_BOUNDARY_BATTERY BATTERY_VOLT_04_200000_V
#define MIN_CURRENT_LIMIT_BOUNDARY_BATTERY BATTERY_VOLT_04_000000_V
#define AC_LESS_N_DEGRESS_CHARGER_CURRENT	CHARGE_CURRENT_200_00_MA  //< 200mA
#endif

#else
#define MAX_CHARGE_TEMPERATURE  50
#define MAX_CHARGE_TEMPERATURE_MINUS_X_DEGREE	47
#define MIN_CHARGE_TEMPERATURE  0
#define MIN_CHARGE_TEMPERATURE_PLUS_X_DEGREE	6
#endif


#define ERR_CHARGE_TEMPERATURE  0xFF

/* Linear Charging Threshold */
#define V_PRE2CC_THRES	 		3400	//mV
#define V_CC2TOPOFF_THRES		4050
#define RECHARGING_VOLTAGE      4310
#define CHARGING_FULL_CURRENT    100	//mA

/* Charging Current Setting */
//#define CONFIG_USB_IF 						   
#define USB_CHARGER_CURRENT_SUSPEND			0		// def CONFIG_USB_IF
#define USB_CHARGER_CURRENT_UNCONFIGURED	CHARGE_CURRENT_70_00_MA	// 70mA
#define USB_CHARGER_CURRENT_CONFIGURED		CHARGE_CURRENT_500_00_MA	// 500mA

#define USB_CHARGER_CURRENT					CHARGE_CURRENT_500_00_MA	//500mA
#define AC_CHARGER_CURRENT					CHARGE_CURRENT_1050_00_MA
//#define AC_CHARGER_CURRENT					CHARGE_CURRENT_1000_00_MA
#define NON_STD_AC_CHARGER_CURRENT			CHARGE_CURRENT_1050_00_MA
#define CHARGING_HOST_CHARGER_CURRENT       CHARGE_CURRENT_650_00_MA
#define APPLE_0_5A_CHARGER_CURRENT          CHARGE_CURRENT_500_00_MA
#define APPLE_1_0A_CHARGER_CURRENT          CHARGE_CURRENT_650_00_MA
#define APPLE_2_1A_CHARGER_CURRENT          CHARGE_CURRENT_800_00_MA


/* Precise Tunning */
#define BATTERY_AVERAGE_DATA_NUMBER	3	
#define BATTERY_AVERAGE_SIZE 	30

/* charger error check */
#ifdef CONFIG_TEMPERATURE_CONTROL_CHARGING
#define BAT_LOW_TEMP_PROTECT_ENABLE         // stop charging if temp < MIN_CHARGE_TEMPERATURE
#else
//#define BAT_LOW_TEMP_PROTECT_ENABLE         // stop charging if temp < MIN_CHARGE_TEMPERATURE
#endif
#define V_CHARGER_ENABLE 0				// 1:ON , 0:OFF	
#define V_CHARGER_MAX 6500				// 6.5 V
#define V_CHARGER_MIN 4400				// 4.4 V

/* Tracking TIME */
#define ONEHUNDRED_PERCENT_TRACKING_TIME	10	// 10 second
#define NPERCENT_TRACKING_TIME	   			20	// 20 second
#define SYNC_TO_REAL_TRACKING_TIME  		60	// 60 second
#define V_0PERCENT_TRACKING							3450 //3450mV

/* Battery Notify */
#define BATTERY_NOTIFY_CASE_0001_VCHARGER
#define BATTERY_NOTIFY_CASE_0002_VBATTEMP
//#define BATTERY_NOTIFY_CASE_0003_ICHARGING
//#define BATTERY_NOTIFY_CASE_0004_VBAT
//#define BATTERY_NOTIFY_CASE_0005_TOTAL_CHARGINGTIME

/* High battery support */
//#define HIGH_BATTERY_VOLTAGE_SUPPORT

/* JEITA parameter */
//#define MTK_JEITA_STANDARD_SUPPORT
#define CUST_SOC_JEITA_SYNC_TIME 30
#define JEITA_RECHARGE_VOLTAGE  4110	// for linear charging
#ifdef HIGH_BATTERY_VOLTAGE_SUPPORT
#define JEITA_TEMP_ABOVE_POS_60_CV_VOLTAGE		BATTERY_VOLT_04_300000_V
#define JEITA_TEMP_POS_45_TO_POS_60_CV_VOLTAGE		BATTERY_VOLT_04_300000_V
#define JEITA_TEMP_POS_10_TO_POS_45_CV_VOLTAGE		BATTERY_VOLT_04_400000_V
#define JEITA_TEMP_POS_0_TO_POS_10_CV_VOLTAGE		BATTERY_VOLT_04_300000_V
#define JEITA_TEMP_NEG_10_TO_POS_0_CV_VOLTAGE		BATTERY_VOLT_04_100000_V
#define JEITA_TEMP_BELOW_NEG_10_CV_VOLTAGE		BATTERY_VOLT_04_100000_V
#else
#define JEITA_TEMP_ABOVE_POS_60_CV_VOLTAGE		BATTERY_VOLT_04_100000_V
#define JEITA_TEMP_POS_45_TO_POS_60_CV_VOLTAGE	BATTERY_VOLT_04_100000_V
#define JEITA_TEMP_POS_10_TO_POS_45_CV_VOLTAGE	BATTERY_VOLT_04_200000_V
#define JEITA_TEMP_POS_0_TO_POS_10_CV_VOLTAGE	BATTERY_VOLT_04_100000_V
#define JEITA_TEMP_NEG_10_TO_POS_0_CV_VOLTAGE	BATTERY_VOLT_03_900000_V
#define JEITA_TEMP_BELOW_NEG_10_CV_VOLTAGE		BATTERY_VOLT_03_900000_V
#endif
/* For JEITA Linear Charging only */
#define JEITA_NEG_10_TO_POS_0_FULL_CURRENT  120	//mA 
#define JEITA_TEMP_POS_45_TO_POS_60_RECHARGE_VOLTAGE  4000
#define JEITA_TEMP_POS_10_TO_POS_45_RECHARGE_VOLTAGE  4100
#define JEITA_TEMP_POS_0_TO_POS_10_RECHARGE_VOLTAGE   4000
#define JEITA_TEMP_NEG_10_TO_POS_0_RECHARGE_VOLTAGE   3800
#define JEITA_TEMP_POS_45_TO_POS_60_CC2TOPOFF_THRESHOLD	4050
#define JEITA_TEMP_POS_10_TO_POS_45_CC2TOPOFF_THRESHOLD	4050
#define JEITA_TEMP_POS_0_TO_POS_10_CC2TOPOFF_THRESHOLD	4050
#define JEITA_TEMP_NEG_10_TO_POS_0_CC2TOPOFF_THRESHOLD	3850


/* For CV_E1_INTERNAL */
#define CV_E1_INTERNAL

/* Disable Battery check for HQA */
#ifdef CONFIG_MTK_DISABLE_POWER_ON_OFF_VOLTAGE_LIMITATION
#define CONFIG_DIS_CHECK_BATTERY
#endif

#ifdef CONFIG_MTK_FAN5405_SUPPORT
#define FAN5405_BUSNUM 1
#endif

#define MTK_PLUG_OUT_DETECTION

#endif /* _CUST_BAT_H_ */ 