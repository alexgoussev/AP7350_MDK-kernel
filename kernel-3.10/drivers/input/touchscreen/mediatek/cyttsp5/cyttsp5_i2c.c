/*
 * cyttsp5_i2c.c
 * Cypress TrueTouch(TM) Standard Product V5 I2C Module.
 * For use with Cypress Txx5xx parts.
 * Supported parts include:
 * TMA5XX
 *
 * Copyright (C) 2012-2014 Cypress Semiconductor
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */
#include "cyttsp5_regs.h"

#include <linux/i2c.h>
#include <linux/version.h>

#ifdef CYTTSP5_I2C_DMA
#define MELFAS_I2C_MASTER_CLOCK 100
#endif

#define CY_I2C_DATA_SIZE  (2 * 256)

#define MTK_ZHWW

#ifdef MTK_ZHWW
#include "../tpd.h"
#include <cust_eint.h>
#include <linux/cdev.h>
#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c/twl.h>
#include <linux/init.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/machine.h>
#include <linux/usb/otg.h>
#include <mach/eint.h>
#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>

/* cyttsp */
#include <linux/cyttsp5_core.h>
#include <linux/cyttsp5_platform.h>
#endif

#ifdef MTK_ZHWW
//Move from mtk_ttda.c
#define CYTTSP5_I2C_TCH_ADR 0x24
#define CYTTSP5_LDR_TCH_ADR 0x24
#define CYTTSP5_I2C_IRQ_GPIO 129 /* J6.9, C19, GPMC_AD14/GPIO_38 */
#define CYTTSP5_I2C_RST_GPIO 56 /* J6.10, D18, GPMC_AD13/GPIO_37 */

#define CYTTSP5_HID_DESC_REGISTER 1

#define CY_VKEYS_X 720 		//MTK_ZHWW Resolution
#define CY_VKEYS_Y 1280
#define CY_MAXX 720
#define CY_MAXY 1280
#define CY_MINX 0
#define CY_MINY 0

#define CY_ABS_MIN_X CY_MINX
#define CY_ABS_MIN_Y CY_MINY
#define CY_ABS_MAX_X CY_MAXX
#define CY_ABS_MAX_Y CY_MAXY
#define CY_ABS_MIN_P 0
#define CY_ABS_MAX_P 255
#define CY_ABS_MIN_W 0
#define CY_ABS_MAX_W 255
#define CY_PROXIMITY_MIN_VAL	0
#define CY_PROXIMITY_MAX_VAL	1

#define CY_ABS_MIN_T 0

#define CY_ABS_MAX_T 15

/* Button to keycode conversion */
static u16 cyttsp5_btn_keys[] = {
	/* use this table to map buttons to keycodes (see input.h) */
	KEY_MENU,		/* 139 */
	KEY_HOMEPAGE,		/* 172 */ /* Previously was KEY_HOME (102) */
				/* New Android versions use KEY_HOMEPAGE */

	KEY_BACK,		/* 158 */
	KEY_SEARCH,		/* 217 */
	KEY_VOLUMEDOWN,		/* 114 */
	KEY_VOLUMEUP,		/* 115 */
	KEY_CAMERA,		/* 212 */
	KEY_POWER		/* 116 */
};

static struct touch_settings cyttsp5_sett_btn_keys = {
	.data = (uint8_t *)&cyttsp5_btn_keys[0],
	.size = ARRAY_SIZE(cyttsp5_btn_keys),
	.tag = 0,
};



static struct cyttsp5_core_platform_data _cyttsp5_core_platform_data = {
//RQIU	
//#ifndef CYTTSP5_IRQ_NO_GPIO
#if 0
	.irq_gpio = CYTTSP5_I2C_IRQ_GPIO,
#endif
	.rst_gpio = CYTTSP5_I2C_RST_GPIO,
	.hid_desc_register = CYTTSP5_HID_DESC_REGISTER,
	.xres = cyttsp5_xres,
	.init = cyttsp5_init,
	.power = cyttsp5_power,
	.detect = cyttsp5_detect,
	.irq_stat = cyttsp5_irq_stat,
	.sett = {
		NULL,	/* Reserved */
		NULL,	/* Command Registers */
		NULL,	/* Touch Report */
		NULL,	/* Cypress Data Record */
		NULL,	/* Test Record */
		NULL,	/* Panel Configuration Record */
		NULL,	/* &cyttsp5_sett_param_regs, */
		NULL,	/* &cyttsp5_sett_param_size, */
		NULL,	/* Reserved */
		NULL,	/* Reserved */
		NULL,	/* Operational Configuration Record */
		NULL, /* &cyttsp5_sett_ddata, *//* Design Data Record */
		NULL, /* &cyttsp5_sett_mdata, *//* Manufacturing Data Record */
		NULL,	/* Config and Test Registers */
		&cyttsp5_sett_btn_keys,	/* button-to-keycode table */
	},
	.flags = CY_CORE_FLAG_WAKE_ON_GESTURE
			| CY_CORE_FLAG_RESTORE_PARAMETERS,
	.easy_wakeup_gesture = CY_CORE_EWG_NONE,
};

static const int16_t cyttsp5_abs[] = {
	ABS_MT_POSITION_X, CY_ABS_MIN_X, CY_ABS_MAX_X, 0, 0,
	ABS_MT_POSITION_Y, CY_ABS_MIN_Y, CY_ABS_MAX_Y, 0, 0,
	ABS_MT_PRESSURE, CY_ABS_MIN_P, CY_ABS_MAX_P, 0, 0,
	CY_IGNORE_VALUE, CY_ABS_MIN_W, CY_ABS_MAX_W, 0, 0,
	ABS_MT_TRACKING_ID, CY_ABS_MIN_T, CY_ABS_MAX_T, 0, 0,
	ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0,
	ABS_MT_TOUCH_MINOR, 0, 255, 0, 0,
	ABS_MT_ORIENTATION, -127, 127, 0, 0,
	ABS_MT_TOOL_TYPE, 0, MT_TOOL_MAX, 0, 0,
	ABS_DISTANCE, 0, 255, 0, 0,	/* Used with hover */
};

struct touch_framework cyttsp5_framework = {
	.abs = cyttsp5_abs,
	.size = ARRAY_SIZE(cyttsp5_abs),
	.enable_vkeys = 0,
};

static struct cyttsp5_mt_platform_data _cyttsp5_mt_platform_data = {
	.frmwrk = &cyttsp5_framework,
	.flags = 0,   //CY_MT_FLAG_FLIP | CY_MT_FLAG_INV_X | CY_MT_FLAG_INV_Y,
	.inp_dev_name = CYTTSP5_MT_NAME,
	.vkeys_x = CY_VKEYS_X,
	.vkeys_y = CY_VKEYS_Y,
};

static struct cyttsp5_btn_platform_data _cyttsp5_btn_platform_data = {
	.inp_dev_name = CYTTSP5_BTN_NAME,
};

static const int16_t cyttsp5_prox_abs[] = {
	ABS_DISTANCE, CY_PROXIMITY_MIN_VAL, CY_PROXIMITY_MAX_VAL, 0, 0,
};

struct touch_framework cyttsp5_prox_framework = {
	.abs = cyttsp5_prox_abs,
	.size = ARRAY_SIZE(cyttsp5_prox_abs),
};

static struct cyttsp5_proximity_platform_data
		_cyttsp5_proximity_platform_data = {
	.frmwrk = &cyttsp5_prox_framework,
	.inp_dev_name = CYTTSP5_PROXIMITY_NAME,
};


// RQIU static struct cyttsp5_platform_data _cyttsp5_platform_data = {
struct cyttsp5_platform_data _cyttsp5_platform_data = {
	.core_pdata = &_cyttsp5_core_platform_data,
	.mt_pdata = &_cyttsp5_mt_platform_data,
	.loader_pdata = &_cyttsp5_loader_platform_data,
	.btn_pdata = &_cyttsp5_btn_platform_data,
	.prox_pdata = &_cyttsp5_proximity_platform_data,
};

static ssize_t cyttsp5_virtualkeys_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf,
		__stringify(EV_KEY) ":"
		__stringify(KEY_MENU) ":1380:120:80:100:"
		__stringify(EV_KEY) ":"
		__stringify(KEY_HOMEPAGE) ":1380:360:80:100:"
		__stringify(EV_KEY) ":"
		__stringify(KEY_BACK) ":1380:600:80:100:\n");		
}


static struct kobj_attribute cyttsp5_virtualkeys_attr = {
	.attr = {
		.name = "virtualkeys.cyttsp5_mt",
		.mode = S_IRUGO,
	},
	.show = &cyttsp5_virtualkeys_show,
};

static struct attribute *cyttsp5_properties_attrs[] = {
	&cyttsp5_virtualkeys_attr.attr,
	NULL
};

static struct attribute_group cyttsp5_properties_attr_group = {
	.attrs = cyttsp5_properties_attrs,
};

static int mtk_ttda_init(void)
{
	struct kobject *properties_kobj;
	int ret = 0;

	printk(KERN_ERR"[cyttsp5] %s,%d\n",__func__,__LINE__);	
	properties_kobj = kobject_create_and_add("board_properties", NULL);
	if (properties_kobj)
		ret = sysfs_create_group(properties_kobj,
				&cyttsp5_properties_attr_group);
	if (!properties_kobj || ret)
		pr_err("%s: failed to create board_properties\n", __func__);

	//tpd_load_status = 1;
	printk(KERN_ERR"[cyttsp5] %s,%d\n",__func__,__LINE__);
	return ret;
}
#endif

#ifdef CYTTSP5_I2C_DMA
static int cyttsp5_i2c_read_default(struct device *dev, void *buf, int size,
		u8 *I2CDMABuf_va, dma_addr_t I2CDMABuf_pa)
#else
static int cyttsp5_i2c_read_default(struct device *dev, void *buf, int size)
#endif
{
	struct i2c_client *client = to_i2c_client(dev);
	int rc;

	if (!buf || !size || size > CY_I2C_DATA_SIZE)
		return -EINVAL;

#ifdef CYTTSP5_I2C_DMA
	client->ext_flag = client->ext_flag | I2C_DMA_FLAG | I2C_ENEXT_FLAG;
	rc = i2c_master_recv(client, (unsigned char *)I2CDMABuf_pa, size);
	memcpy(buf, I2CDMABuf_va, size);
	client->ext_flag = client->ext_flag
		& (~I2C_DMA_FLAG) & (~I2C_ENEXT_FLAG);
#else
	rc = i2c_master_recv(client, buf, size);
#endif

	return (rc < 0) ? rc : rc != size ? -EIO : 0;
}

#ifdef CYTTSP5_I2C_DMA
static int cyttsp5_i2c_read_default_nosize(struct device *dev, u8 *buf, u32 max,
		u8 *I2CDMABuf_va, dma_addr_t I2CDMABuf_pa)
#else
static int cyttsp5_i2c_read_default_nosize(struct device *dev, u8 *buf, u32 max)
#endif
{
	struct i2c_client *client = to_i2c_client(dev);
	struct i2c_msg msgs[2];
	u8 msg_count = 1;
	int rc;
	u32 size;

	if (!buf)
		return -EINVAL;

#if 0
	msgs[0].addr = client->addr;
	msgs[0].flags = (client->flags & I2C_M_TEN) | I2C_M_RD;
	msgs[0].len = 2;
#ifdef CYTTSP5_I2C_DMA
	msgs[0].ext_flag = client->ext_flag
		& (~I2C_DMA_FLAG) & (~I2C_ENEXT_FLAG);
#endif
	msgs[0].buf = buf;
	rc = i2c_transfer(client->adapter, msgs, msg_count);
	if (rc < 0 || rc != msg_count)
		return (rc < 0) ? rc : -EIO;
#endif

	client->ext_flag = client->ext_flag | I2C_DMA_FLAG | I2C_ENEXT_FLAG;
	rc = i2c_master_recv(client, (unsigned char *)I2CDMABuf_pa, 2);
	memcpy(buf, I2CDMABuf_va, 2);
	client->ext_flag = client->ext_flag
			& (~I2C_DMA_FLAG) & (~I2C_ENEXT_FLAG);
	
	if (rc < 0 || rc != 2)
		return (rc < 0) ? rc : -EIO;

	size = get_unaligned_le16(&buf[0]);
	if (!size || size == 2)
		return 0;

	if (size > max)
		return -EINVAL;

#ifdef CYTTSP5_I2C_DMA
	client->ext_flag = client->ext_flag | I2C_DMA_FLAG | I2C_ENEXT_FLAG;
	rc = i2c_master_recv(client, (unsigned char *)I2CDMABuf_pa, size);
	memcpy(buf, I2CDMABuf_va, size);
	client->ext_flag = client->ext_flag
		& (~I2C_DMA_FLAG) & (~I2C_ENEXT_FLAG);
#else
	rc = i2c_master_recv(client, buf, size);
#endif

	return (rc < 0) ? rc : rc != (int)size ? -EIO : 0;
}


#ifdef CYTTSP5_I2C_DMA
static int cyttsp5_i2c_write_read_specific(struct device *dev, u8 write_len,
		u8 *write_buf, u8 *read_buf,
		u8 *I2CDMABuf_va, dma_addr_t I2CDMABuf_pa)
#else
static int cyttsp5_i2c_write_read_specific(struct device *dev, u8 write_len,
		u8 *write_buf, u8 *read_buf)
#endif
{
	struct i2c_client *client = to_i2c_client(dev);
	struct i2c_msg msgs[2];
	u8 msg_count = 1;
	int rc;

	if (!write_buf || !write_len)
		return -EINVAL;

	msgs[0].addr = client->addr;
	msgs[0].flags = client->flags & I2C_M_TEN;
	msgs[0].len = write_len;
#ifdef CYTTSP5_I2C_DMA
	msgs[0].buf = (u8 *)I2CDMABuf_pa;
	msgs[0].ext_flag = client->ext_flag | I2C_DMA_FLAG | I2C_ENEXT_FLAG;
	msgs[0].timing = MELFAS_I2C_MASTER_CLOCK;
	memcpy(I2CDMABuf_va, write_buf, write_len);
#else
	msgs[0].buf = write_buf;
#endif
	rc = i2c_transfer(client->adapter, msgs, msg_count);

	if (rc < 0 || rc != msg_count)
		return (rc < 0) ? rc : -EIO;

	rc = 0;

	if (read_buf)
#ifdef CYTTSP5_I2C_DMA
		rc = cyttsp5_i2c_read_default_nosize(dev, read_buf,
				CY_I2C_DATA_SIZE, I2CDMABuf_va, I2CDMABuf_pa);
#else
		rc = cyttsp5_i2c_read_default_nosize(dev, read_buf,
				CY_I2C_DATA_SIZE);
#endif

	return rc;
}

static struct cyttsp5_bus_ops cyttsp5_i2c_bus_ops = {
	.bustype = BUS_I2C,
	.read_default = cyttsp5_i2c_read_default,
	.read_default_nosize = cyttsp5_i2c_read_default_nosize,
	.write_read_specific = cyttsp5_i2c_write_read_specific,
};

/*power on for the wdt without reset pin*/
int wdt_power_on(void)
{	
	int ret = 0;

    msleep(100);
    
    hwPowerDown(MT6328_POWER_LDO_VGP1, "TP");

    mdelay(100);
        
    hwPowerOn(MT6328_POWER_LDO_VGP1, VOL_2800, "TP");

	msleep(100);
		
//	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
//    msleep(100);
	
	return ret;
}

static int cyttsp5_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *i2c_id)
{
	struct device *dev = &client->dev;
	int rc;
	//char *buf[50]={0};
		
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(dev, "I2C functionality not Supported\n");
		return -1;
	}

	rc = cyttsp5_probe(&cyttsp5_i2c_bus_ops, &client->dev, client->irq,
			  CY_I2C_DATA_SIZE);
	printk(KERN_ERR"[cyttsp5] %s,%d,rc = %d\n",__func__,__LINE__,rc);
	return rc;
}

static int cyttsp5_i2c_remove(struct i2c_client *client)
{
	struct cyttsp5_core_data *cd = i2c_get_clientdata(client);

	cyttsp5_release(cd);

	return 0;
}

static const struct i2c_device_id cyttsp5_i2c_id[] = {
	{ CYTTSP5_I2C_NAME, 0, },
	{ }
};

static unsigned short force[] = {0, 0x24, I2C_CLIENT_END, I2C_CLIENT_END};
static const unsigned short *const forces[] = { force, NULL };
MODULE_DEVICE_TABLE(i2c, cyttsp5_i2c_id);

static struct i2c_driver cyttsp5_i2c_driver = {
	.driver = {
		.name = CYTTSP5_I2C_NAME,
		.owner = THIS_MODULE,
		.pm = &cyttsp5_pm_ops,
	},
	.probe = cyttsp5_i2c_probe,
	.remove = cyttsp5_i2c_remove,
	.id_table = cyttsp5_i2c_id,
	.address_list = (const unsigned short *) forces,
};


static int tpd_local_init(void)
{


	if(i2c_add_driver(&cyttsp5_i2c_driver)!=0)
	{
		TPD_DMESG("unable to add i2c driver.\n");
		return -1;
	}

	//TPD_DEBUG("pancong tpd_load_status = %d,ft5406 driver.\n",tpd_load_status);

	if(-1 == tpd_load_status)
	{
		i2c_del_driver(&cyttsp5_i2c_driver);
		TPD_DEBUG("[pancong] del ft5406_i2c_driver successful\n");

		return -1;
	}

	TPD_DMESG("end %s, %d\n", __FUNCTION__, __LINE__);  
	tpd_type_cap = 1;

	return 0; 
}

static int tpd_resume(struct i2c_client *client)
{
	int retval = 0;
	char data = 0;
	int retry_num = 0,ret = 0;

	TPD_DEBUG("TPD wake up\n");
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);  
	return retval;
}
 
static int tpd_suspend(struct i2c_client *client, pm_message_t message)
{
	int retval = 0;
	TPD_DEBUG("TPD enter sleep\n");
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);	
	return retval;
} 

static ssize_t show_chipinfo(struct device *dev,struct device_attribute *attr, char *buf)
{
     s32 ret = -1;
	 u8 temp_data = 0;	 
	 u8  buff[16];	 
	 u8  pid[8];			  
	 u16 vid = 0;				
	 u16 version_info;
	 
	struct i2c_client *client =to_i2c_client(dev);
	if(NULL == client)
	{
		//GTP_ERROR("i2c client is null!!\n");
		return 0;
	}
	return sprintf(buf, "IC: cyttsp5\n");
}


static DEVICE_ATTR(chipinfo, 0444, show_chipinfo, NULL);

static const struct device_attribute * const ctp_attributes[] = {
	&dev_attr_chipinfo
};

static struct tpd_driver_t tpd_device_driver =
{
    .tpd_device_name = CYTTSP5_I2C_NAME,
    .tpd_local_init = tpd_local_init,
    .suspend = tpd_suspend,
    .resume = tpd_resume,
#ifdef TPD_HAVE_BUTTON
    .tpd_have_button = 1,
#else
    .tpd_have_button = 0,
#endif
    .attrs = 
    {
        .attr = ctp_attributes,
        .num  = ARRAY_SIZE(ctp_attributes),
    },
};



#ifdef MTK_ZHWW
//Move from mtk_ttda.c

static struct i2c_board_info ttda_i2c_devices[] __initdata = {
	{
		I2C_BOARD_INFO(CYTTSP5_I2C_NAME, CYTTSP5_I2C_TCH_ADR),
		.irq = -1,
		.platform_data = &_cyttsp5_platform_data,
	},
};

#endif

#if 0
static int __init cyttsp5_i2c_init(void)
{
	#ifdef MTK_ZHWW
	//Move from mtk_ttda.c
	//printk(KERN_ERR"[cyttsp5 zhww] %s,%d\n",__func__,__LINE__);	
	i2c_register_board_info(1, ttda_i2c_devices,
			ARRAY_SIZE(ttda_i2c_devices));

if (tpd_driver_add(&tpd_device_driver) < 0)
        printk("add generic driver failed\n");
	
	mtk_ttda_init();
	//printk(KERN_ERR"[cyttsp5 zhww] %s,%d\n",__func__,__LINE__);	
	#endif
	#if 0
	int rc = i2c_add_driver(&cyttsp5_i2c_driver);
	
	pr_info("%s: Cypress TTSP v5 I2C Driver (Built %s) rc=%d\n",
		 __func__, CY_DRIVER_DATE, rc);
	return rc;
	#endif
	return 0;
}

static void __exit cyttsp5_i2c_exit(void)
{
    GTP_INFO("MediaTek gt91xx touch panel driver exit\n");
    tpd_driver_remove(&tpd_device_driver);
}

#else
static int __init tpd_driver_init(void)
{
    i2c_register_board_info(1, &ttda_i2c_devices, 1);
    if (tpd_driver_add(&tpd_device_driver) < 0)
     printk("add generic driver failed\n");
	
    return 0;
}

/* should never be called */
static void __exit tpd_driver_exit(void)
{
    tpd_driver_remove(&tpd_device_driver);
}
#endif

module_init(tpd_driver_init);
module_exit(tpd_driver_exit);



MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cypress TrueTouch(R) Standard Product I2C driver");
MODULE_AUTHOR("Cypress Semiconductor <ttdrivers@cypress.com>");
