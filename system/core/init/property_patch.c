/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/mtkfb.h>
#include <linux/mtkfb_info.h>

#include "property_service.h"
#include "log.h"


// should follow frameworks/base/core/java/android/util/DisplayMetrics.java definition
typedef enum
{
    LCD_DENSITY_LOW   = 0,
    LCD_DENSITY_MEDIUM,
    LCD_DENSITY_TV,
    LCD_DENSITY_HIGH,
    LCD_DENSITY_XHIGH,
    LCD_DENSITY_400,
    LCD_DENSITY_XXHIGH,
    LCD_DENSITY_XXXHIGH,
    MAX_LCD_DENSITY,
} LCD_DENSITY_TYPE;


static unsigned int g_lcd_density[MAX_LCD_DENSITY] = 
{
    120, // LCD_DENSITY_LOW
    160, // LCD_DENSITY_MEDIUM
    213, // LCD_DENSITY_TV
    240, // LCD_DENSITY_HIGH
    320, // LCD_DENSITY_XHIGH
    400, // LCD_DENSITY_400
    480, // LCD_DENSITY_XXHIGH
    640, // LCD_DENSITY_XXXHIGH
};


int patch_lcd_density(void)
{
	char tmp[PROP_VALUE_MAX];
    const char FB_DEV[] = "/dev/graphics/fb0";
    const char LCD_DENSITY_PROP[] = "ro.sf.lcd_density";
    char value[10];
    
    struct fb_var_screeninfo vinfo;
    float xdpi;
    float ydpi;
    unsigned int i;
    unsigned int pixels;
    unsigned int default_density = 160;
    int fd = -1;
    int ret = 0;

    /* check if lcd_density has been defined */
    if (property_get(LCD_DENSITY_PROP, tmp)) goto done;

    if ((fd = open(FB_DEV, O_RDONLY)) < 0) {
        ERROR("[ERROR] failed to open %s", FB_DEV);
        ret = -1;
        goto done;
    }

    if ((ret = ioctl(fd, FBIOGET_VSCREENINFO, &vinfo)) < 0) {
        ERROR("[ERROR] failed to get fb_var_screeninfo");
        goto done;
    }


    if((int)vinfo.width <= 0 || (int)vinfo.height <= 0) 
    {
        pixels = vinfo.xres * vinfo.yres;
        if (pixels <= 240 * 432) default_density = 120;         // <= WQVGA432
        else if ((pixels >= 480 * 800) && (pixels <=1024 * 600)) default_density = 240;    // >= WVGA854
        else if ((pixels > 1024 * 600) && (pixels <1920 * 1080))default_density = 320;//720p
        else if (pixels >= 1920 * 1080)default_density = 480; //FHD

    } 
    else 
    {
        xdpi = vinfo.xres * 25.4f / vinfo.width;
        ydpi = vinfo.yres * 25.4f / vinfo.height;

        if (xdpi <= g_lcd_density[LCD_DENSITY_LOW])
        {
            xdpi = g_lcd_density[LCD_DENSITY_LOW];
        }
        else
        {
            for (i=LCD_DENSITY_LOW; i<(MAX_LCD_DENSITY-1); i++)
            {
                if ((xdpi > g_lcd_density[i]) && (xdpi <= g_lcd_density[i+1]))
                {
                    if (xdpi <= ((g_lcd_density[i] + g_lcd_density[i+1])/2))
                    {
                        default_density = g_lcd_density[i];
                    }
                    else
                    {
                        default_density = g_lcd_density[i+1];
                    }
                    break;
                }
            }
            if (i == (MAX_LCD_DENSITY-1))
            {
                default_density = g_lcd_density[i];
            }
        }
    }

    sprintf(value, "%d", default_density);
    if ((ret = property_set(LCD_DENSITY_PROP, value)) < 0) {
        ERROR("[ERROR] failed to set property %s = %s", LCD_DENSITY_PROP, value);
        goto done;
    }

done:
    close(fd);
    return ret;
}


int patch_properties(void)
{
    int ret = 0;

    if ((ret = patch_lcd_density()) < 0) {
        ERROR("[ERROR] patch lcd_density property failed");
        return ret;
    }

    return ret;
}

