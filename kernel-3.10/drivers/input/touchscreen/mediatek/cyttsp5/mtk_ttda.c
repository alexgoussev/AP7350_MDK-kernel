/*
 * mtk_ttda.c
 * Cypress TrueTouch(TM) Standard Product V4 MTK support module.
 * For use with Cypress Txx4xx parts.
 * Supported parts include:
 * TMA4XX
 * TMA1036
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

#define MTK_ZHWW

static int __init ttda_driver_init(void)
{
	pr_info("MediaTek TTDA init\n");
	return 0;
}

/* should never be called */
static void __exit ttda_driver_exit(void)
{

}

module_init(ttda_driver_init);
module_exit(ttda_driver_exit);
