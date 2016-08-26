#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
	#include <platform/upmu_common.h>
	#include <platform/upmu_hw.h>

	#include <platform/mt_gpio.h>
	#include <platform/mt_i2c.h> 
	#include <platform/mt_pmic.h>
	#include <string.h>
#else
	#include <mach/mt_pm_ldo.h>	/* hwPowerOn */
	#include <mach/upmu_common.h>
	#include <mach/upmu_sw.h>
	#include <mach/upmu_hw.h>

	#include <mach/mt_gpio.h>
#endif
#include <cust_gpio_usage.h>
#ifndef CONFIG_FPGA_EARLY_PORTING
#include <cust_i2c.h>
#endif
static LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)    								(lcm_util.set_reset_pin((v)))
#define MDELAY(n) 											(lcm_util.mdelay(n))
#define UDELAY(n) 											(lcm_util.udelay(n))


// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)										lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   			lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)    

#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>  
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
//#include <linux/jiffies.h>
#include <linux/uaccess.h>
//#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
/***************************************************************************** 
 * Define
 *****************************************************************************/
#endif

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------
#define LCM_DSI_CMD_MODE  0

#define FRAME_WIDTH  										(480)
#define FRAME_HEIGHT 										(800)

#ifndef CONFIG_FPGA_EARLY_PORTING
#define GPIO_65132_EN GPIO_LCD_BIAS_ENP_PIN
#endif

#define REGFLAG_DELAY             								0xFC
#define REGFLAG_UDELAY             								0xFB
#define REGFLAG_END_OF_TABLE      							    0xFD   // END OF REGISTERS MARKER


#define OTM8018B_DSI_VDO_XIND_ID  (0x8009)

static LCM_DSI_MODE_SWITCH_CMD lcm_switch_mode_cmd;

#ifndef TRUE
    #define TRUE 1
#endif

#ifndef FALSE
    #define FALSE 0
#endif

#ifndef BUILD_LK
static unsigned int lcm_esd_test = FALSE;      ///only for ESD test
#endif
// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------



#define _LCM_DEBUG_

#ifdef BUILD_LK
#define printk printf
#endif

#ifdef _LCM_DEBUG_
#define lcm_debug(fmt, args...) printk(fmt, ##args)
#else
#define lcm_debug(fmt, args...) do { } while (0)
#endif

#ifdef _LCM_INFO_
#define lcm_info(fmt, args...) printk(fmt, ##args)
#else
#define lcm_info(fmt, args...) do { } while (0)
#endif
#define lcm_err(fmt, args...) printk(fmt, ##args)

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

struct LCM_setting_table {
    unsigned char cmd;
    unsigned char count;
    unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28,0,{}},
	{0x10,0,{}},
	{REGFLAG_DELAY, 120, {}},
	{0x4F,1,{0x01}},
	{REGFLAG_DELAY, 120, {}}
};

static struct LCM_setting_table lcm_init_setting[] = {
	
	/*
	Note :

	Data ID will depends on the following rule.
	
		count of parameters > 1	=> Data ID = 0x39
		count of parameters = 1	=> Data ID = 0x15
		count of parameters = 0	=> Data ID = 0x05

	Structure Format :

	{DCS command, count of parameters, {parameter list}}
	{REGFLAG_DELAY, milliseconds of time, {}},

	...

	Setting ending by predefined flag
	
	{REGFLAG_END_OF_TABLE, 0x00, {}}
	*/
    {0xBF,   3, {0x91,0x61,0xF2}}, //SET password, 91/61/F2 can access all page

    {0xB3,   2, {0x00,0x74}}, //SET VCOM  //VCOM 可调84改善

    {0xB4,   2, {0x00,0x6A}},//SET VCOM_R

    {0xB8,   6, {0x00,0xBF,0x01,0x00,0xBF,0x01}}, //VGMP, VGSP, VGMN, VGSN

    {0xBA,   3, {0x34,0x23,0x00}},

    {0xC3,   1, {0x02}},

    {0xC4,   2, {0x00,0x64}},

    {0xC7,   9, {0x00,0x01,0x31,0x0A,0x6A,0x2C,0x13,0xA5,0xA5}},

    {0xC8,  38, {0x7E,0x61,0x4F,0x42,0x3E,0x2E,0x32,0x1C,0x34,0x32
                ,0x31,0x50,0x3F,0x48,0x3B,0x38,0x2B,0x1C,0x01,0x7E
                ,0x61,0x4F,0x42,0x3E,0x2E,0x32,0x1C,0x34,0x32,0x31
                ,0x50,0x3F,0x48,0x3B,0x38,0x2B,0x1C,0x01}},

    {0xD4,  16, {0x1F,0x1E,0x05,0x07,0x01,0x1F,0x1F,0x1F,0x1F,0x1F
                ,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F}},

    {0xD5,  16, {0x1F,0x1E,0x04,0x06,0x00,0x1F,0x1F,0x1F,0x1F,0x1F
                ,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F}},

    {0xD6,  16, {0x1F,0x1F,0x06,0x04,0x00,0x1E,0x1F,0x1F,0x1F,0x1F
                ,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F}},

    {0xD7,  16, {0x1F,0x1F,0x07,0x05,0x01,0x1E,0x1F,0x1F,0x1F,0x1F
                ,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F}},

    {0xD8,  20, {0x20,0x00,0x00,0x10,0x03,0x20,0x01,0x02,0x00,0x01
                ,0x02,0x36,0x4F,0x00,0x00,0x32,0x04,0x36,0x4F,0x08}},

    {0xD9,  19, {0x00,0x0A,0x0A,0x88,0x00,0x00,0x06,0x7B,0x00,0x00
                ,0x00,0x3B,0x2F,0x1F,0x00,0x00,0x00,0x03,0x7B}},

    {0x35, 1, {0x00}},
    {0x3A, 1, {0x77}},

    {0x11, 0, {0x00}},
    {REGFLAG_DELAY, 120, {}},
    {0x29, 0, {0x00}},

    {REGFLAG_DELAY, 50, {}},
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_sleep_out_setting[] = {
    // Sleep Out
	{0x11, 0, {0x00}},
    {REGFLAG_DELAY, 150, {}},

    // Display ON
	{0x29, 0, {0x00}},
	{REGFLAG_DELAY, 20, {}},
	
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_sleep_in_setting[] = {
	// Display off sequence
	{0x28, 0, {0x00}},
	 {REGFLAG_DELAY, 10, {}},
	
      // Sleep Mode On
	{0x10, 0, {0x00}},
         {REGFLAG_DELAY, 150, {}},

	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
    unsigned int i;

    for(i = 0; i < count; i++)
    {
        unsigned cmd;
        cmd = table[i].cmd;

        switch (cmd) {

            case REGFLAG_DELAY :
                MDELAY(table[i].count);
                break;
				
			case REGFLAG_UDELAY :
				UDELAY(table[i].count);
				break;

            case REGFLAG_END_OF_TABLE :
                break;

            default:
                dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
        }
    }
}

// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
    memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}


static void lcm_get_params(LCM_PARAMS *params)
{
	memset(params, 0, sizeof(LCM_PARAMS));
	
	params->type   = LCM_TYPE_DSI;

	params->width  = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

    // enable tearing-free
    params->dbi.te_mode                 = LCM_DBI_TE_MODE_DISABLED;
    params->dbi.te_edge_polarity        = LCM_POLARITY_RISING;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode   = CMD_MODE;
#else
	params->dsi.mode   = SYNC_PULSE_VDO_MODE;//BURST_VDO_MODE; 
#endif
	
	// DSI
	/* Command mode setting */
	//1 Three lane or Four lane
	    
    params->dsi.LANE_NUM				= LCM_TWO_LANE;
		
    //The following defined the fomat for data coming from LCD engine.
	params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;
	params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;
	params->dsi.packet_size=256;

	// Video mode setting		
	params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active				= 4;
	params->dsi.vertical_backporch					= 6;
	params->dsi.vertical_frontporch					= 8;
	params->dsi.vertical_active_line				= FRAME_HEIGHT; 

	params->dsi.horizontal_sync_active				= 10;
	params->dsi.horizontal_backporch				= 60;
	params->dsi.horizontal_frontporch				= 60;
	params->dsi.horizontal_active_pixel				= FRAME_WIDTH;

    //params->dsi.LPX=8; 

	// Bit rate calculation
	params->dsi.PLL_CLOCK = 200;

	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd          = 0x53;
	params->dsi.lcm_esd_check_table[0].count        = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x24;

}

static void lcm_init_power(void)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef BUILD_LK
	pmic_set_register_value(PMIC_RG_VGP1_EN,1);
#else
	printk("%s, begin\n", __func__);
	hwPowerOn(MT6328_POWER_LDO_VGP1, VOL_DEFAULT, "LCM_DRV");	
	printk("%s, end\n", __func__);
#endif
#endif
}

static void lcm_suspend_power(void)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef BUILD_LK
	pmic_set_register_value(PMIC_RG_VGP1_EN,0);
#else
	printk("%s, begin\n", __func__);
	hwPowerDown(MT6328_POWER_LDO_VGP1, "LCM_DRV");	
	printk("%s, end\n", __func__);
#endif
#endif
}

static void lcm_resume_power(void)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef BUILD_LK
	pmic_set_register_value(PMIC_RG_VGP1_EN,1);
#else
	printk("%s, begin\n", __func__);
	hwPowerOn(MT6328_POWER_LDO_VGP1, VOL_DEFAULT, "LCM_DRV");	
	printk("%s, end\n", __func__);
#endif
#endif
}


static void lcm_init(void)
{
	unsigned char cmd = 0x0;
	unsigned char data = 0xFF;
	int ret=0;
	cmd=0x00;
	data=0x0A;
//data=0x0A;VSP=5V,//data=0x0E;VSP=5.4V


	
	lcm_debug("luosen %s %d\n", __func__,__LINE__);
		SET_RESET_PIN(0);
		MDELAY(20); 
		SET_RESET_PIN(1);
		MDELAY(20); 

       // init_lcm_registers();
       push_table(lcm_init_setting, sizeof(lcm_init_setting) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_suspend(void)
{
	push_table(lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);  
	//SET_RESET_PIN(0);
	MDELAY(10);
}

static void lcm_resume(void)
{
	lcm_init(); 
}
         
#if (LCM_DSI_CMD_MODE)
static void lcm_update(unsigned int x, unsigned int y,
                       unsigned int width, unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0>>8)&0xFF);
	unsigned char x0_LSB = (x0&0xFF);
	unsigned char x1_MSB = ((x1>>8)&0xFF);
	unsigned char x1_LSB = (x1&0xFF);
	unsigned char y0_MSB = ((y0>>8)&0xFF);
	unsigned char y0_LSB = (y0&0xFF);
	unsigned char y1_MSB = ((y1>>8)&0xFF);
	unsigned char y1_LSB = (y1&0xFF);

	unsigned int data_array[16];

	data_array[0]= 0x00053902;
	data_array[1]= (x1_MSB<<24)|(x0_LSB<<16)|(x0_MSB<<8)|0x2a;
	data_array[2]= (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);
	
	data_array[0]= 0x00053902;
	data_array[1]= (y1_MSB<<24)|(y0_LSB<<16)|(y0_MSB<<8)|0x2b;
	data_array[2]= (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0]= 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);

}
#endif

static unsigned int lcm_compare_id(void)
{
	char  buffer;
	unsigned int data_array[2];

	data_array[0]= 0x00043902;
	data_array[1]= (0x94<<24)|(0x83<<16)|(0xff<<8)|0xb9;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]= 0x00023902;
	data_array[1]= (0x33<<8)|0xba;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0]= 0x00043902;
	data_array[1]= (0x94<<24)|(0x83<<16)|(0xff<<8)|0xb9;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00013700;
	dsi_set_cmdq(data_array, 1, 1);

	read_reg_v2(0xf4, &buffer, 1);

	#ifdef BUILD_LK
		printf("%s, LK debug: hx8394d id = 0x%08x\n", __func__, buffer);
    #else
		printk("%s, kernel debug: hx8394d id = 0x%08x\n", __func__, buffer);
    #endif

	return 1;//(buffer == HX8394D_HD720_ID ? 1 : 0);

}


static unsigned int lcm_esd_check(void)
{
  #ifndef BUILD_LK

	if(lcm_esd_test)
	{
		lcm_esd_test = FALSE;
		return TRUE;
	}

	char  buffer;
	read_reg_v2(0x0a, &buffer, 1);
	printk("%s, kernel debug: reg = 0x%08x\n", __func__, buffer);

	return FALSE;
	
#else
	return FALSE;
#endif

}

static unsigned int lcm_esd_recover(void)
{
	lcm_init();

	return TRUE;
}


static void* lcm_switch_mode(int mode)
{
#ifndef BUILD_LK
//customization: 1. V2C config 2 values, C2V config 1 value; 2. config mode control register
	if(mode == 0)
	{//V2C
		lcm_switch_mode_cmd.mode = CMD_MODE;
		lcm_switch_mode_cmd.addr = 0xBB;// mode control addr
		lcm_switch_mode_cmd.val[0]= 0x13;//enabel GRAM firstly, ensure writing one frame to GRAM
		lcm_switch_mode_cmd.val[1]= 0x10;//disable video mode secondly
	}
	else
	{//C2V
		lcm_switch_mode_cmd.mode = SYNC_PULSE_VDO_MODE;
		lcm_switch_mode_cmd.addr = 0xBB;
		lcm_switch_mode_cmd.val[0]= 0x03;//disable GRAM and enable video mode
	}
	return (void*)(&lcm_switch_mode_cmd);
#else
	return NULL;
#endif
}



LCM_DRIVER otm8018b_dsi_vdo_xind_lcm_drv = 
{
    .name			= "otm8018b_dsi_vdo_xind",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id     = lcm_compare_id,
     .init_power		= lcm_init_power,
     .resume_power = lcm_resume_power,
     .suspend_power = lcm_suspend_power,
	//.esd_check 	= lcm_esd_check,
	//.esd_recover	= lcm_esd_recover,
#if (LCM_DSI_CMD_MODE)
    .update         = lcm_update,
#endif
};
