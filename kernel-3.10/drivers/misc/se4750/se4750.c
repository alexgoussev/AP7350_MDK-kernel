
/*
 * Symbol SE4750 imaging module driver
 *
 * Copyright (C) 2012 Motorola solutions
 * Copyright (C) 2012 MM Solutions
 *
 * Author Stanimir Varbanov <svarbanov@mm-sol.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * Raghav - Restructed se4750 to use se4750_command to remove redudancy.
 * Read back AutoLowPower to actually enter into Low power mode.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/log2.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include "se4750.h"

#define SE4750_SIZE_WVGA            0
#define SE4750_I2C_DEVICE_ADDR      0x5c
#define SE4750_ACQ                  0x58
#define SE4750_ACQ_ON               0x01
#define SE4750_ACQ_OFF              0x00
#define SE4750_AIM                  0x55
#define SE4750_AIM_ON               0x01
#define SE4750_AIM_OFF              0x00
#define SE4750_AIM_DURING_EXPOSURE  0x56
#define SE4750_ILLUM                0x59
#define SE4750_ILLUM_ON             0x01
#define SE4750_ILLUM_OFF            0x00
#define SE4750_AUTO_POWER           0x74
#define SE4750_AUTO_POWER_EN        0x01
#define SE4750_TIME_TO_LOW_POWER    0x75

#define SE4750_I2C_TIMING           200

static u8 *SE4750_I2CDMABuf_va = NULL;
static u32 SE4750_I2CDMABuf_pa = NULL;

static struct i2c_board_info __initdata se4750_i2c_info={ I2C_BOARD_INFO("moto_sdl", 0x5c)};

struct se4750 {
    struct i2c_client * pClient;
    int i_size;
    int i_fmt;
    atomic_t open_excl;
};

struct se4750_i2c_msg {
    __u16 addr; /* slave address            */
    __u16 flags;
#define I2C_M_TEN       0x0010  /* this is a ten bit chip address */
#define I2C_M_RD        0x0001  /* read data, from slave to master */
#define I2C_M_NOSTART       0x4000  /* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_REV_DIR_ADDR  0x2000  /* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_IGNORE_NAK    0x1000  /* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_NO_RD_ACK     0x0800  /* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_RECV_LEN      0x0400  /* length will be first received byte */
    __u16 len;      /* msg length               */
    __u8 *buf;      /* pointer to msg data          */
};

/*
 * struct se4750
 *
 *  Main structure for storage of sensor information
 *
 * @pdata:      Access functions and data for platform level information
 * @ver:        SE4750 chip version TODO: does the SE4750 have this?
 * @model:      Model number returned during detect
 * @power:      Turn the interface ON or OFF
 */
struct se4750_dev
{
    struct se4750_platform_data*    pdata;
    int                             ver;
    char                            model[SE45PARAM_MODELNO_LEN + 1];
    char                            abSN[SE45PARAM_SERIALNO_LEN + 5];
    unsigned int                    power;
};

// WA for se4750_misc dev moto_sdl
static struct se4750* se4750_misc =  NULL;

static struct se4750_dev   SE4750Dev;
static struct se4750_dev*   pSE4750Dev = &SE4750Dev;

static int se4750_i2c_write( struct i2c_client *client, char* buf, int dw_len)
{

    int i = 0;
    for(i = 0 ; i < dw_len; i++)
    {
        SE4750_I2CDMABuf_va[i] = buf[i];

        printk("se4750_i2c WRITE I2C SE4750_I2CDMABuf_va[%d] = %d\n", i,buf[i]);
    }

    if(dw_len <= 8)
    {
        client->addr = client->addr & I2C_MASK_FLAG;
        //MSE_ERR("Sensor non-dma write timing is %x!\r\n", this_client->timing);
        return i2c_master_send(client, buf, dw_len);
    }
    else
    {
        client->addr = client->addr & I2C_MASK_FLAG | I2C_DMA_FLAG;
        //MSE_ERR("Sensor dma timing is %x!\r\n", this_client->timing);
        return i2c_master_send(client, SE4750_I2CDMABuf_pa, dw_len);
    }
}


static int se4750_i2c_read(struct i2c_client *client, char *buf, int len)
{
    int i = 0, err = 0;
    int retry = 0;
    printk("se4750_i2c READ I2C CMD BUF = %d , len = %d\n", *buf ,len);

    for(;retry < 5;retry++)
    {
        if(len < 8)
        {
            client->addr = client->addr & I2C_MASK_FLAG;
            //MSE_ERR("Sensor non-dma read timing is %x!\r\n", this_client->timing);
            err = i2c_master_recv(client, buf, len);
        }
        else
        {
            client->addr = client->addr & I2C_MASK_FLAG | I2C_DMA_FLAG;
            //MSE_ERR("Sensor dma read timing is %x!\r\n", this_client->timing);
            err = i2c_master_recv(client, SE4750_I2CDMABuf_pa, len);

            if(err >= 0)
            { 
                for(i = 0; i < len; i++)
                {
                    buf[i] = SE4750_I2CDMABuf_va[i];
                }
            }
        }
        if(err < 0)
            udelay(500);
        else
            break;
    }
    return err;
}


static int se4750_i2c_transfer(struct i2c_client *client, struct i2c_msg *msgs, int num)
{
    client->timing = SE4750_I2C_TIMING;
    if(msgs->flags &I2C_M_RD)
    {
        se4750_i2c_read(client, msgs->buf, msgs->len);

    }
    else
    {
        se4750_i2c_write(client, msgs->buf,msgs->len);
    }

}

static int se4750_read_ext(struct i2c_client* pClient, u8* data, int data_length)
{
    struct i2c_msg  msg;
    int             err;

    msg.addr   = pClient->addr;
    msg.flags  = I2C_M_RD;
    msg.len    = data_length;
    msg.buf    = data;
    err         = se4750_i2c_transfer(pClient, &msg, 1);
    if ( err >= 0 )
    {
        err = 0;    // Success
    }
    return(err);
}

/*
 * se4750_write - Write a command to the SE4750 device
 * @pClient:        i2c driver client structure
 * @data:           pointer to data to write
 * @data_length:    length of data to write
 *
 * Write a command to the SE4750 device.
 * Returns zero if successful, or non-zero otherwise.
 */
static int se4750_write_ext(struct i2c_client* pClient, u8* data, int data_length)
{
    struct i2c_msg  msg;
    int             err;

    msg.addr   = pClient->addr;
    msg.flags  = 0;
    msg.len    = data_length;
    msg.buf    = data;
    err         = se4750_i2c_transfer(pClient, &msg, 1);
    if ( err >= 0 )
    {
        err = 0;    // Non-negative indicates success
    }

    return(err);
}

static int se4750_command_ext(struct i2c_client* pClient, u8 bCmd, u8* pParam, int num_param, u8* pResp, int resp_len)
{
    int retVal;
    int iIdx;
    int iCmdLen;
    u8  abCmd[SE45_MAX_CMD_LEN];
    u8  bCkSum;
    u8  bTmp;

    // Make sure command, params and checksum will fit in buffer
    if ( (num_param >= (sizeof(abCmd) - 2)) || (num_param < 0) )
    {
        return(-EINVAL);
    }

    // Build command and calculate checksum
    abCmd[0] = bCkSum = bCmd;
    for ( iIdx = 0; iIdx < num_param; )
    {
        bTmp = pParam[iIdx++];
        abCmd[iIdx] = bTmp;
        bCkSum += bTmp;
    }
    abCmd[++iIdx] = -bCkSum;    // Store checksum

    iCmdLen = num_param + 2;
    retVal = -EIO;

    // Try up to 3 times to send the command
    for ( iIdx = 0; iIdx < 3; ++iIdx )
    {
        retVal = se4750_write_ext(pClient, abCmd, iCmdLen);
        if ( 0 == retVal )
        {
            // Command successfully sent
            // Try up to 3 times to read the response
            for ( iIdx = 0; iIdx < 3; ++iIdx )
            {
                msleep(5);
                retVal = se4750_read_ext(pClient, pResp, resp_len);
                if ( 0 == retVal )
                {
                    // TODO: Should we check for ACK?
                    return(resp_len);
                    //return(retVal);
                }
            }
            //v4l_err(pClient, "Read %02X response failed, err=%d\n", bCmd, retVal);
            return(retVal);
        }
    }
    //v4l_err(pClient, "Write %02X failed, err=%d\n", bCmd, retVal);
    return(retVal);
}

/*
 * se4750_detect - Detect if an SE4750 is present, and if so get the model number
 * @pSubdev:    pointer to the V4L2 sub-device driver structure
 *
 * Detect if an SE4750 is present
 * Returns a negative error number if no device is detected, or 0x99
 * if the model number is successfully read.
 */
int se4750_detect(void)
{
    struct se4750_dev*  pSE4750 = pSE4750Dev;
    struct i2c_client*  pClient = NULL;
    int retVal;
    int numParam;
    u8  abParam[2];
    u8  abResp[SE45PARAM_MODELNO_LEN + 4];


    if (se4750_misc != NULL)
        pClient = se4750_misc->pClient;
    else
        return 0;

    // Start and stop acquisition as a workaround for the AIM not working first time problem
    numParam = 1;
    abParam[0] = 1;
    // Uncomment the following to get a visual confirmation that I2C is working at startup
    retVal = se4750_command_ext(pClient, SE45OP_ARMACQUISITION, abParam, numParam, abResp, 2);
    abParam[0] = 0;
    retVal = se4750_command_ext(pClient, SE45OP_ARMACQUISITION, abParam, numParam, abResp, 2);

    // Try to get the model number from the sensor
    numParam = 2;
    abParam[0] = (SE45PARAM_MODELNO & 0x00FF) >> 0;
    abParam[1] = (SE45PARAM_MODELNO & 0xFF00) >> 8;
    retVal = se4750_command_ext(pClient, SE45OP_GETPARAM, abParam, numParam, abResp, sizeof(abResp));
    if ( retVal > 0 )
    {
        memcpy(pSE4750->model, abResp + 4, sizeof(pSE4750->model) - 1);
        printk("se4750 model=%s\n", (char*)pSE4750->model);
        if(strstr((char*)pSE4750->model,"4750") != NULL){
            // SE-4750DL-I000R
            return 0x4750;
        }
    }
    else
    {
        u8  abSN[SE45PARAM_SERIALNO_LEN + 5];

        abParam[0] = (SE45PARAM_SERIALNO & 0x00FF) >> 0;
        abParam[1] = (SE45PARAM_SERIALNO & 0xFF00) >> 8;
        memset(abSN, 0, sizeof(abSN));
        retVal = se4750_command_ext(pClient, SE45OP_GETPARAM, abParam, numParam, abSN, sizeof(abSN) - 1);
        if ( retVal > 0 )
        {
            printk("se4750 S/N=%s\n", (char*) abSN);
            memcpy(pSE4750->abSN, abSN, sizeof(pSE4750->abSN) - 1);
        }
    }

    printk("se4750 model=%s\n", (char*)pSE4750->model);
    return -1;
}

int se4750_start_stream(int streaming){
    struct i2c_client*  pClient = se4750_misc->pClient;

    // If the 'misc' device is open, do not attempt to enable or disable acquisition
    int retVal;
    int numParam;
    u8  abResp[2];
    u8  abParam[1];

    numParam = 1;
    if ( streaming ){
#if 0 
        // If streaming is being turned on, set the acquisition mode for imaging and enable illumination
        //abParam[0] = ACQMODE_IMAGE;
        //retVal = se4750_command_ext(pClient, SE45OP_ACQUISITIONMODE, abParam, numParam, abResp, sizeof(abResp));
        //printk("se4750 set SE45OP_ACQUISITIONMODE retVal = %d \n", retVal);
        
        abParam[0] = 1;
        retVal = se4750_command_ext(pClient, SE45OP_ILLUMDURINGEXPOSURE, abParam, numParam, abResp, sizeof(abResp));
        printk("se4750 set SE45OP_ILLUMDURINGEXPOSURE retVal = %d \n", retVal);
        
        abParam[0] = 1;
        retVal = se4750_command_ext(pClient, SE45OP_AIM, abParam, numParam, abResp, sizeof(abResp));
        printk("se4750 set SE45OP_AIM retVal = %d \n", retVal);
#endif
    }
    // Turn acquisition on or off
    abParam[0] = streaming ? 1 : 0;
    retVal = se4750_command_ext(pClient, SE45OP_ARMACQUISITION, abParam, numParam, abResp, sizeof(abResp));
    printk("se4750 set SE45OP_ARMACQUISITION retVal = %d \n", retVal);

    printk("se4750 set stream %s called\n", streaming ? "on" : "off");

    return(0);
}

int se4750_stop_stream(){
#if 0
    struct i2c_client*  pClient = se4750_misc->pClient;

    int retVal;
    int numParam;
    u8  abParam[1];
    u8  abResp[SE45PARAM_MODELNO_LEN + 4];

    printk("se4750 set stop stream \n");

    abParam[0] = 0;
    numParam = 1;
    retVal = se4750_command_ext(pClient,SE45OP_ARMACQUISITION, abParam, numParam, abResp, sizeof(abResp));

    abParam[0] = 0;
    numParam = 1;
    retVal = se4750_command_ext(pClient,SE45OP_AIM, abParam, numParam, abResp, sizeof(abResp));

    abParam[0] = 0;
    numParam = 1;
    retVal = se4750_command_ext(pClient,SE45OP_ILLUMDURINGEXPOSURE, abParam, numParam, abResp, sizeof(abResp));
#endif
}

static int se4750_init(struct i2c_client *client)
{
    dev_dbg(&client->dev, "Sensor initialized\n");

    return 0;
}

// Support for moto_sdl to be exposed to the IAL
static int se4750_misc_open(struct inode* node, struct file* file)
{
    int i ;
    
    if ( atomic_inc_return(&se4750_misc->open_excl) != 1 )
    {
        atomic_dec(&se4750_misc->open_excl);
        return -EBUSY;
    }
  
    file->private_data = se4750_misc;

    return(0);
}

static long se4750_misc_ioctl(struct file* file, unsigned int cmd, unsigned long arg)
{
    struct se4750* se4750;
    struct i2c_rdwr_ioctl_data rdwr_data;
    struct se4750_i2c_msg msg;
    u8 __user* usr_data;
    int ret = 0;
    

    se4750 = file->private_data;
    
    if ( (se4750 == NULL) || (cmd != I2C_RDWR) || !arg ) {
        return -EINVAL;
    }

    if ( copy_from_user(&rdwr_data, (struct i2c_rdwr_ioctl_data __user*) arg, sizeof(rdwr_data)) ) {
        return -EFAULT;
    }

    if ( rdwr_data.nmsgs != 1 ) {
        return -EINVAL;
    }

    if ( copy_from_user(&msg, rdwr_data.msgs, sizeof(struct se4750_i2c_msg)) ) {
        return -EFAULT;
    }

    // Only allow transfers to the SE4750, limit the size of the message and don't allow received length changes
    if ( (msg.addr != SE4750_I2C_DEVICE_ADDR) || (msg.len > 256) || (msg.flags & I2C_M_RECV_LEN) ) {
        return -EINVAL;
    }

    // Map the data buffer from user-space to kernel space
    // WA reuse same structure for message
    usr_data = (u8 __user*) msg.buf;
    msg.buf = memdup_user(usr_data, msg.len);
    if ( IS_ERR(msg.buf) )
    {
        return(PTR_ERR(msg.buf));
    }

    ret = se4750_i2c_transfer(se4750->pClient, &msg, 1);
    if ( (ret >= 0) && (msg.flags & I2C_M_RD) ) {
        // Successful read, copy data to user-space
        if ( copy_to_user(usr_data, msg.buf, msg.len) ) {
            ret = -EFAULT;
        }
    }

    kfree(msg.buf);
    return ret;
}

static int se4750_misc_release(struct inode* node, struct file* file)
{
    atomic_dec(&se4750_misc->open_excl);
    return(0);
}

static const struct file_operations se4750_misc_fops =
{
    .owner = THIS_MODULE,
    .unlocked_ioctl = se4750_misc_ioctl,
    .open = se4750_misc_open,
    .release = se4750_misc_release,
};

static struct miscdevice se4750_misc_device =
{
    .minor = MISC_DYNAMIC_MINOR,
    .name = "moto_sdl",
    .fops = &se4750_misc_fops,
};

static int se4750_probe(struct i2c_client *client,
            const struct i2c_device_id *did)
{
    struct se4750 *se4750;
    int ret;

    SE4750_I2CDMABuf_va = (u8 *)dma_alloc_coherent(&client->dev, 4096, &SE4750_I2CDMABuf_pa, GFP_KERNEL);
    if(!SE4750_I2CDMABuf_va)
    {
        printk("se4750 [TSP] dma_alloc_coherent error\n");
        return -ENOMEM;
    }

    se4750 = kzalloc(sizeof(*se4750), GFP_KERNEL);
    if (!se4750)
    {
        printk("\r\n se4750 allocation fails\n");
        return -ENOMEM;
    }
    
    se4750->i_size = SE4750_SIZE_WVGA;
    se4750->i_fmt = 0; /* First format in the list */
    se4750->pClient = client ;

    ret = se4750_init(client);
    if (ret) {
        dev_err(&client->dev, "Failed to initialize sensor\n");
        ret = -EINVAL;
    }

    se4750_misc = se4750;
    printk("se4750 %s: atomic_set\n", __func__);
    atomic_set(&se4750_misc->open_excl, 0);

    printk("se4750 %s: misc_register\n", __func__);
    misc_register(&se4750_misc_device);

    printk("se4750 %s: sucess\n", __func__);

    return ret;
}

static int se4750_remove(struct i2c_client *client)
{
    struct se4750 *se4750 = se4750_misc ;
    misc_deregister(&se4750_misc_device);

    client->driver = NULL;
    kfree(se4750);

    if(SE4750_I2CDMABuf_va)
    {
        dma_free_coherent(&client->dev, 4096, SE4750_I2CDMABuf_va, SE4750_I2CDMABuf_pa);
        SE4750_I2CDMABuf_va = NULL;
        SE4750_I2CDMABuf_pa = 0;
    }
    se4750_misc= NULL;

    return 0;
}

static const struct i2c_device_id se4750_id[] = {
    { "moto_sdl", 0 },
    { }
};

MODULE_DEVICE_TABLE(i2c, se4750_id);

static struct i2c_driver se4750_i2c_driver = {
    .driver = {
        .name = "moto_sdl",
    },
    .probe = se4750_probe,
    .remove = se4750_remove,
    .id_table = se4750_id,
};

static int __init se4750_mod_init(void)
{
    printk("se4750 :  MediaTek  se4750  driver init\n");
    i2c_register_board_info(0, &se4750_i2c_info, 1);
    return i2c_add_driver(&se4750_i2c_driver);
}

static void __exit se4750_mod_exit(void)
{
    i2c_del_driver(&se4750_i2c_driver);
}

module_init(se4750_mod_init);
module_exit(se4750_mod_exit);

MODULE_DESCRIPTION("Symbol SE4750 Imaging module driver");
MODULE_AUTHOR("Stanimir Varbanov <svarbanov@mm-sol.com>");
MODULE_LICENSE("GPL v2");

