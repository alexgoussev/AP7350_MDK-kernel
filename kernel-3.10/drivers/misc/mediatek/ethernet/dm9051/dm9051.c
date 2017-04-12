/* drivers/net/dm9051.c (dm9r.c )
 *
 * Copyright 2014 Davicom Semiconductor,Inc.
 *	http://www.davicom.com.tw
 *	2014/03/11  Joseph CHANG  v1.0  Create	
 *	2014/05/05  Joseph CHANG  v1.01  Start from Basic functions	
 *	2015/06/08  Joseph CHANG  v1.02  Formal naming (Poll version)	
 *
 * 	This program is free software; you can redistribute it and/or
 * 	modify it under the terms of the GNU General Public License
 * 	as published by the Free Software Foundation; either version 2
 * 	of the License, or (at your option) any later version.
 *
 * 	This program is distributed in the hope that it will be useful,
 * 	but WITHOUT ANY WARRANTY; without even the implied warranty of
 * 	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * 	GNU General Public License for more details.
 *
 *  Ver: Step1.2p1: Use "db->ret95" to trace bug track.. (20151127)
 *  Ver: Step1.3 dma: mt_ DMA.. (20151203)
 *  Ver: Step1.3p1 DMA3_PNs design for platforms' config (20151223)
  * Ver: 3p6s
  * Ver: 3p6ss
  * Ver: 3p11sss (kmalloc re-placement)
  *      Remove: drwbuff_u8()
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/cache.h>
#include <linux/crc32.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/irqreturn.h>
#include <linux/spi/spi.h>

 #define DMA3_P0  1
 #define DMA3_P1  1
 #define DMA3_P3  1
 #define DMA3_P4  1
 #define DMA3_P6  1  // 0: dm9051.ko, 1: dm9051.o (module or static)
 #define DMA3_P5  0  // 0: 27 MHz, 1: 25M MHz (for maximize spi_speed)
 
#define DMA3_P2_MSEL_MOD	1 // SPI_MODE, 0: FIFO, 1: DMA (conjunction with 1024,32,1 bytes or no-limitation)
#define DMA3_P2_RSEL_1024F	1 // RD_MACH: FIFO model (1024 bytes-limitation)
#define DMA3_P2_RSEL_32F	0 // RD_MACH: FIFO model (32 bytes-limitation)
#define DMA3_P2_RSEL_1F		0 // RD_MACH: FIFO model (1 byte-limitation)

#define DMA3_P2_TSEL_1024F	1 // TX_MACH: FIFO model (1024 bytes-limitation)
#define DMA3_P2_TSEL_32F	0 // TX_MACH: FIFO model (32 bytes-limitation)
#define DMA3_P2_TSEL_1F		0 // TX_MACH: FIFO model (1 byte-limitation)

#define GPIO_DM9051_POWER_PIN   GPIO62

/* ------------------------------- */
/* - ReadWrite.configuration.table */
/* ------------------------------- */

//SPI_MODE
#if DMA3_P2_MSEL_MOD
 #define MSTR_MOD_VERSION		"(SPI_Master is DMA mode..)"   //enhance
#else
 #define MSTR_MOD_VERSION		"(SPI_Master is FIFO mode..)"  //def
#endif
//RD_MACH
#if DMA3_P2_RSEL_1024F
  #define RD_MODEL_VERSION		"(1024 bytes-limitation)"
#elif DMA3_P2_RSEL_32F
  #define RD_MODEL_VERSION		"(32 bytes-limitation)"	   //test.OK
#elif DMA3_P2_RSEL_1F
  #define RD_MODEL_VERSION		"(1 byte-limitation)"
#else
  #define RD_MODEL_VERSION		"(no-limitation)"		   //best
#endif
//TX_MACH
#if DMA3_P2_TSEL_1024F
  #define WR_MODEL_VERSION		"(1024 bytes-limitation)"
#elif DMA3_P2_TSEL_32F
  #define WR_MODEL_VERSION		"(32 bytes-limitation)"	   //tobe.test.again
#elif DMA3_P2_TSEL_1F
  #define WR_MODEL_VERSION		"(1 byte-limitation)"	   //tobe.test.again
#else
  #define WR_MODEL_VERSION		"(no-limitation)"		   //best
#endif



#if DMA3_P0
/*3p*/
#include <mach/mt_spi.h>
#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>
#include <linux/gpio.h>
#include <mach/eint.h>
#include <cust_eint.h>
#include <linux/poll.h>
#include <linux/interrupt.h>
/* */
#endif
#if DMA3_P6
/* Joseph 20151030 */
extern int spi_register_board_info(struct spi_board_info const *info, unsigned n);
#endif

/* 3p6s */
#define PRINTKR  1  // or define to 0, to disable the print 
#define PRINTKT  1  // or define to 0, to disable the print 

#if PRINTKR
  #define printkr	printk
#else
  #define printkr	printk_dumm_3p6sss
#endif

#if PRINTKT
  #define printkt	printk
#else
  #define printkt	printk_dumm_3p6sss
#endif

/* 3p6s.s */
/*int printk_dumm_3p6s(const char *fmt, ...);*/
//asmlinkage __visible int printk_dumm_3p6sss(const char *fmt, ...)
//{
 // return 0; 
  /* 3p6s.empty */
//}
//EXPORT_SYMBOL(printk_dumm_3p6sss);
/* 3p6s.e */

#define CMMB_SPI_INTERFACE_MAX_PKT_LENGTH_PER_TIMES (0x400)
#include "dm9051.h"

/* Board/System/Debug information/definition ---------------- */

#define DRV_INTERRUPT_1                 0
#define DRV_POLL_0                      1

#if DRV_INTERRUPT_1
#define DRV_VERSION	\
	"3.0.8KT_R7.JJ0.INTWrk"
#endif
#if DRV_POLL_0
#define DRV_VERSION	\
	"4.1.11-V7+.3p1.2015.POLL-mt_dma"
#endif

#define DS_NUL							0
#define DS_POLL							1
#define DS_IDLE							2
#define CCS_NUL							0
#define CCS_PROBE						1

#define SPI_SYNC_TRANSFER_BUF_LEN	(4 + DM9051_PKT_MAX)

//Choice one of below, otherwise 100FDX
#define DBG_TEST_10MHDX                 0
#define DBG_TEST_10MFDX                 0
#define DBG_TEST_100MHDX                0

#define DBG_TO_FINAL_ADD_1_INDEED 		1
#define DBG_TO_FINAL_ADD_0_INDEED 		0

//.static int msg_enable;
#define DBG_YN							0
#define BREG_ON							0
#define TXRX_STRUCT_ON					0 
	
// Local compiler-option
/*  RX is basic essential */
/*  TX is made optional('DM9051_CONF_TX') */
#define DM9051_CONF_TX   				1
#define TRACE_XLOOP						1
#define LOOP_XMIT						1
#define SCH_XMIT						0 

#if LOOP_XMIT
static char *str_drv_xmit_type= \
    "LOOP_XMIT";
#endif
#if SCH_XMIT
static char *str_drv_xmit_type= \
    "sch_xmit";
#endif

#if DM9051_CONF_TX
#define NUM_QUEUE_TAIL					0xFFFE   //(2) //(5)//(65534= 0xFFFE)MAX_QUEUE_TAIL  
#endif

//#define CONF_OK_DMA		1

static struct spi_board_info dm9051_spi_board_devs[] __initdata = {
	[0] = {
	.modalias = "dm9051",
#if DMA3_P5
	.max_speed_hz = 25 * 1000 *1000,  // 25MHz
#else	
	.max_speed_hz = 34 * 1000 *1000,  // MAX 27MHz
#endif
	.bus_num = 0,
	.chip_select = 0,
	.mode = SPI_MODE_0,
#if DRV_INTERRUPT_1	  
	.irq             = 3,
#endif		
	},
};
/*
struct mt_chip_conf spi_dma_conf_mt65xx = {
	.setuptime = 15,
	.holdtime = 15,
	.high_time = 10,       //10--6m   15--4m   20--3m  30--2m  [ 60--1m 120--0.5m  300--0.2m]
	.low_time = 10,
	.cs_idletime = 20,

	.rx_mlsb = 1, 
	.tx_mlsb = 1,		 
	.tx_endian = 0,
	.tx_endian = 0,

	.cpol = 0,
	.cpha = 0,
	.com_mod = DMA_TRANSFER,

	.pause = 0,
	.finish_intr = 1,
	.deassert = 0,
};

struct mt_chip_conf spi_fifo_conf_mt65xx = {
	.setuptime =15,
	.holdtime = 15,
	.high_time = 10,  //   ó?low_time  12í????¨×?SPI CLK μ??ü?ú
	.low_time = 10,
	.cs_idletime = 20,
	
	.rx_mlsb = 1,       
	.tx_mlsb = 1,  
	//mlsb=1  ±íê???bit???è′?￡?í¨3￡2?Dèòa??
	.tx_endian = 0,     //tx_endian  =1 ±íê?′ó???￡ê?￡???óúDMA?￡ê?￡?Dèòa?ù?Yéè±?μ?Spec à′????￡?í¨3￡?a′ó???￡
	.rx_endian = 0,
	
	.cpol = 0,      //  ?aà?2?Dèòa?ùéè?? ￡?  xxxx_spi_client->mode = SPI_MODE_0 ?aà?éè???′?é
	.cpha = 0,     //   ?aà?2?Dèòa?ùéè?? ￡?  xxxx_spi_client->mode = SPI_MODE_0 ?aà?éè???′?é
	
	//spi_par->com_mod = DMA_TRANSFER;     // DMA  or FIFO
	.com_mod = FIFO_TRANSFER,
	.pause = 0,     //ó? deassert  μ?òa???à·′?￡?′ê?·??§3??Yí￡?￡ê?￡?è?′?×?SPI_CS ?ú?à′?transfer???? 2??á±?de-active
	
	.finish_intr = 1,  
	//spi_par->deassert = 0;
	.ulthigh = 0,
	.tckdly = 0,
};
*/

void dm9051_power_en(int enable)
{
    if(enable){
        mt_set_gpio_mode(GPIO_DM9051_POWER_PIN,GPIO_MODE_00);
        mt_set_gpio_dir(GPIO_DM9051_POWER_PIN,GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_DM9051_POWER_PIN,GPIO_OUT_ONE);

        mdelay(100); /* delay needs by DM9051 */
    }else{
        mt_set_gpio_mode(GPIO_DM9051_POWER_PIN,GPIO_MODE_00);
        mt_set_gpio_dir(GPIO_DM9051_POWER_PIN,GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_DM9051_POWER_PIN,GPIO_OUT_ZERO);
    }
}

void wbuff_u8(u8 op, u8 *txb)
{
	txb[0]= op;
}

void wbuff(unsigned op, __le16 *txb)
{
  //op= DM_SPI_WR | reg | val
	txb[0] = cpu_to_le16(op);
}

/* DM9051 basic structures ---------------------------- */

struct rx_ctl_mach {
	__le16	RxLen;
	u16		ERRO_counter; /*cycFOUND_counter*/
	u16		RXBErr_counter;
	u16		LARGErr_counter;
	u16		StatErr_counter; /* new */
    u16		FIFO_RST_counter; /*(preRST_counter) / cycRST_counter*/
    
	u16		rxbyte_counter;
	u16		rxbyte_counter0;
	
	u16		rxbyte_counter0_to_prt;
#if 0	
	u16		rx_brdcst_counter; 
	u16		rx_multi_counter;
#endif 	
	u16		rx_unicst_counter;
	u8		isbyte; // ISR Register data
	u8		dummy_pad;
};

#if DMA3_P1 
/*3p*/
int SPI_GPIO_Set(int enable)
{
	
	if(enable)
		{
			mt_set_gpio_mode(GPIO_SPI_CS_PIN, 1);
			mt_set_gpio_pull_enable(GPIO_SPI_CS_PIN, GPIO_PULL_ENABLE);
			mt_set_gpio_pull_select(GPIO_SPI_CS_PIN, GPIO_PULL_UP);
			
			mt_set_gpio_mode(GPIO_SPI_SCK_PIN, 1);
			mt_set_gpio_pull_enable(GPIO_SPI_SCK_PIN, GPIO_PULL_ENABLE);
			mt_set_gpio_pull_select(GPIO_SPI_SCK_PIN, GPIO_PULL_DOWN);
			
			mt_set_gpio_mode(GPIO_SPI_MISO_PIN, 1);
			mt_set_gpio_pull_enable(GPIO_SPI_MISO_PIN, GPIO_PULL_ENABLE);
			mt_set_gpio_pull_select(GPIO_SPI_MISO_PIN, GPIO_PULL_DOWN);
			
			mt_set_gpio_mode(GPIO_SPI_MOSI_PIN, 1);
			mt_set_gpio_pull_enable(GPIO_SPI_MOSI_PIN, GPIO_PULL_ENABLE);
			mt_set_gpio_pull_select(GPIO_SPI_MOSI_PIN, GPIO_PULL_DOWN);
			//sublcd_msg("CMMB GPIO CS SPI PIN mode:num:%d, %d,out:%d, dir:%d,pullen:%d,pullup%d",GPIO_SPI_CS_PIN,mt_get_gpio_mode(GPIO_SPI_CS_PIN),mt_get_gpio_out(GPIO_SPI_CS_PIN),
			//mt_get_gpio_dir(GPIO_SPI_CS_PIN),mt_get_gpio_pull_enable(GPIO_SPI_CS_PIN),mt_get_gpio_pull_select(GPIO_SPI_CS_PIN));    
		}
	else
		{
			mt_set_gpio_mode(GPIO_SPI_CS_PIN, 0);
			mt_set_gpio_dir(GPIO_SPI_CS_PIN, GPIO_DIR_IN);
			mt_set_gpio_pull_enable(GPIO_SPI_CS_PIN, GPIO_PULL_DISABLE);
			
			mt_set_gpio_mode(GPIO_SPI_SCK_PIN, 0);
			mt_set_gpio_dir(GPIO_SPI_SCK_PIN, GPIO_DIR_IN);
			mt_set_gpio_pull_enable(GPIO_SPI_SCK_PIN, GPIO_PULL_DISABLE);
			
			mt_set_gpio_mode(GPIO_SPI_MISO_PIN, 0);
			mt_set_gpio_dir(GPIO_SPI_MISO_PIN, GPIO_DIR_IN);
			mt_set_gpio_pull_enable(GPIO_SPI_MISO_PIN, GPIO_PULL_DISABLE);
			
			mt_set_gpio_mode(GPIO_SPI_MOSI_PIN, 0);
			mt_set_gpio_dir(GPIO_SPI_MOSI_PIN, GPIO_DIR_IN);
			mt_set_gpio_pull_enable(GPIO_SPI_MOSI_PIN, GPIO_PULL_DISABLE);
		}
	

	return 0;
	
}
/* */
#endif
static void bcprobe_rst_info_clear(struct rx_ctl_mach *pbc)
{
	pbc->ERRO_counter= 
	pbc->RXBErr_counter= 
	pbc->LARGErr_counter= 
	pbc->StatErr_counter=
	pbc->FIFO_RST_counter= 0;
}
static void bcopen_rx_info_clear(struct rx_ctl_mach *pbc)
{
	pbc->rxbyte_counter= 
	pbc->rxbyte_counter0= 
	
	pbc->rxbyte_counter0_to_prt= 
#if 0	
	pbc->rx_brdcst_counter= 
	pbc->rx_multi_counter= 
#endif	
	pbc->rx_unicst_counter= 0;
	
	pbc->isbyte= 0xff; // Special Pattern
}
static void bcrdy_rx_info_clear(struct rx_ctl_mach *pbc)
{
	pbc->rxbyte_counter= 
	pbc->rxbyte_counter0= 0;
}

struct tx_state_mach {
	u16		prob_cntStopped;
	char	local_cntTXREQ;
	char	pad_0;
	u16		local_cntLOOP; // for trace looptrace_rec[]
#if TRACE_XLOOP
	#define NUMLPREC  16
	struct loop_tx {
	  u16 	looptrace_rec[NUMLPREC];  	// 20140522
	  int	loopidx;					// 20140522
	} dloop_tx;
#endif
};

#if TRACE_XLOOP	
void dloop_init(struct loop_tx *pt)
{
	int i;
    for (i=0; i<NUMLPREC; i++)
    {
      pt->looptrace_rec[i]= 0; // means no_value
    }
    pt->loopidx= 0;
}

void dloop_step(struct loop_tx *pt, u16 n)
{
	int i;
	if (pt->loopidx==NUMLPREC)
	{
    	pt->loopidx--; //63
    	//shift
	  	for (i=1; i<NUMLPREC; i++)
	  	  pt->looptrace_rec[i-1]= pt->looptrace_rec[i];
	}
    pt->looptrace_rec[pt->loopidx]= n;
    pt->loopidx++;
}

void dloop_diap_ALL(struct loop_tx *pt)
{
	int i;
	//display ALL: refer to rec_s.u.b_d.i.s.p(pdrec, tx_space);
	printk("TrXmitLP[%d]", pt->loopidx);
	if (!pt->loopidx) {
		printk("\n");
		return;
	}
	
	for (i=0; i<pt->loopidx; i++)
	{
	  if (!(i%8)) printk(" ");
	  else printk(",");
	  printk("%d", pt->looptrace_rec[i]); //rec_rxdisp(prx, i);
	  //if (pt->looptrace_rec[i]=='w') printk(" ");
	}
	printk("\n");
	
	dloop_init(pt);
}
#endif

/* Structure/enum declaration ------------------------------- */
/*
static const struct ethtool_ops dm9051_ethtool_ops = {
	.get_link			= dm9000_get_link,
	static u32 dm9000_get_link(struct net_device *dev)
	{
	   ret = mii_link_ok(&dm->mii);
	   //= ret = dm9051_read_locked(dm, DM9051_NSR) & NSR_LINKST ? 1 : 0;
*/

typedef struct board_info {
	u8 tmpTxPtr[CMMB_SPI_INTERFACE_MAX_PKT_LENGTH_PER_TIMES];
	u8 tmpRxPtr[CMMB_SPI_INTERFACE_MAX_PKT_LENGTH_PER_TIMES];

	struct net_device   *ndev; /* dm9051's netdev */
	struct spi_device	*spidev;

	struct rx_ctl_mach	  bC;
	struct tx_state_mach  bt;
	u8					driver_state;
	u8					chip_code_state;
	
	u16 				rwregs[2];
	u16 				DISPLAY_rwregs[2];
	u16 DERFER_calc;
	u16 DERFER_rwregs[2];
	
	u16 				tx_rwregs[2];
	u16 				tx_qs;
	
	int					tx_eq;
	int					tx_err;
	
	u16 				rx_count;
	
	u8					imr_all;
	u8					rcr_all;
	int					link;

	unsigned int		in_suspend :1;
	unsigned int		wake_supported :1;
	int					ip_summed;

	struct mutex	 	addr_lock;	/* dm9051's lock;*/	/* phy and eeprom access lock */
	struct mutex	 	sublcd_mtkspi_mutex; //Uesd when is 'SPI_DMA_MTK' of 'CONF_OK_DMA'
#if 1	
	spinlock_t			statelock; /* state lock;*/
#endif
	u8					rxd[8] ____cacheline_aligned; //aS R1 for R2.R3
	u8					txd[8];
	u32					msg_enable ____cacheline_aligned;

	struct work_struct	tx_work;
	struct delayed_work	phy_poll;
	struct work_struct	rxctrl_work;
#if DRV_INTERRUPT_1 | DRV_POLL_0
	struct delayed_work	rx_work; //"INT_or_poll_Work; & TX_Work;"
  //struct work_struct	rx_work;
#endif

	struct mii_if_info 	mii;
	struct sk_buff_head	txq;

	struct spi_message	spi_msg1;
	struct spi_transfer	spi_xfer1;

	struct spi_message	spi_dmsg1; //Uesd when is 'SPI_DMA_MTK' of 'CONF_OK_DMA'
	struct spi_transfer	spi_dxfer1; //Uesd when is 'SPI_DMA_MTK'
	struct spi_transfer	spi_dxfer2; //Uesd when is 'SPI_DMA_MTK'

	u8  TxDatBuf[SPI_SYNC_TRANSFER_BUF_LEN];
} board_info_t;

/* use ethtool to change the level for any given device */
static struct {
	u32 msg_enable;
} debug = { -1 };


void driver_dtxt_init(board_info_t *db)	{
	// func: dtxt_init(&db->bt.dtxt_tx); eliminated
}
void driver_dtxt_step(board_info_t *db, char c){
     // func: dtxt_step(&db->bt.dtxt_tx, c); eliminated
}
void driver_dtxt_disp(board_info_t *db){ //'=dm9051_queue_show'   
	// func: dtxt_diap_ALL(&db->bt.dtxt_tx); eliminated
}

void driver_dloop_init(board_info_t *db){
	#if TRACE_XLOOP	
	dloop_init(&db->bt.dloop_tx);
	#endif
}
void driver_dloop_step(board_info_t *db, u16 n)
{
	#if TRACE_XLOOP
	dloop_step(&db->bt.dloop_tx, n);
	#endif
}
void driver_dloop_disp(board_info_t *db){
	#if TRACE_XLOOP
	dloop_diap_ALL(&db->bt.dloop_tx);
	#endif
}

/* DM9051 basic routines ---------------------------- */

static inline board_info_t *to_dm9051_board(struct net_device *dev)
{
	return netdev_priv(dev);
}

//board_info_t;
#if DRV_INTERRUPT_1
static irqreturn_t dm9051_interrupt(int irq, void *dev_id);
#endif
#if DRV_INTERRUPT_1 | DRV_POLL_0
static void dm9051_continue_poll(struct work_struct *work); // old. dm9051_INTP_isr()
#endif

#if DRV_INTERRUPT_1 | DRV_POLL_0
static void dm9051_INTPschedule_isr(board_info_t *db);
//static irqreturn_t dm9051_isr(int irq, void *dev_id); // discard
static irqreturn_t dm9051_isr_ext(int irq, void *dev_id, int flag);
#endif

static int  dm9051_phy_read(struct net_device *dev, int phy_reg_unused, int reg);
static void dm9051_phy_write(struct net_device *dev, int phyaddr_unused, int reg, int value);
static void dm9051_init_dm9051(struct net_device *dev);
static void dm9051_fifo_reset(u8 state, u8 *hstr, board_info_t *db);
static void dm9051_fifo_reset_statistic(board_info_t *db);

/* DM9051 basic spi_sync call ---------------------------- */

#define RD_LEN_ONE	1

void xwrbyte(board_info_t * db, __le16 *txb) //xwrbuff
{
	struct spi_transfer *xfer = &db->spi_xfer1;
	struct spi_message *msg = &db->spi_msg1;
	int ret;
	
	xfer->tx_buf = txb;
	xfer->rx_buf = NULL;
	xfer->len = 2;

	ret = spi_sync(db->spidev, msg);
	if (ret < 0)
		netdev_err(db->ndev, "spi_.sync()failed (xwrbyte 0x%04x)\n", txb[0]);
}

void xrdbyte(board_info_t * db, __le16 *txb, u8 *trxb)
{
	struct spi_transfer *xfer= &db->spi_xfer1;
	struct spi_message *msg= &db->spi_msg1;
	int ret;
	unsigned rxl= 1;
	
		xfer->tx_buf = txb;
		xfer->rx_buf = trxb;
		xfer->len = rxl + 1;

		ret = spi_sync(db->spidev, msg);
		if (ret < 0)
			netdev_err(db->ndev, "xrdbyte 0x%04x: spi_.sync()fail ret= %d\n", txb[0], ret);
//u8	return trxb[1];
}

void xrdbuff_u8(board_info_t *db, u8 *txb, u8 *trxb, unsigned len)
{
	struct spi_transfer *xfer = &db->spi_xfer1;
	struct spi_message *msg = &db->spi_msg1;
	int ret;
	
		//(One byte)
        xfer->tx_buf = txb;
        xfer->rx_buf = trxb;
        xfer->len = RD_LEN_ONE + len;
		ret = spi_sync(db->spidev, msg);
		if (ret < 0){
	    	printk("9051().e out.in.dump_fifo4, %d BYTES, ret=%d\n", len, ret); //"%dth byte", i
			printk(" <failed.e>\n");
		}
//u8	return trxb[1];
}

static int INNODev_sync(board_info_t *db)
{
	int ret;
	mutex_lock(&db->sublcd_mtkspi_mutex);
	
	ret= spi_sync(db->spidev, &db->spi_dmsg1);
		
	mutex_unlock(&db->sublcd_mtkspi_mutex);
	if(ret){
		printk("[dm95_spi] spi.sync fail ret= %d, should check", ret);
		if(ret == -ETIME){
		}
	}		 
	return ret;
}
#if 0
void dwrite_buff_u8(board_info_t *db, u8 *txb, u8 *trxb, int len)
{
	struct spi_transfer *xfer;
  //struct spi_message *msg;
	int const pkt_count = (len + 1)/ CMMB_SPI_INTERFACE_MAX_PKT_LENGTH_PER_TIMES;
	int const remainder = (len + 1)% CMMB_SPI_INTERFACE_MAX_PKT_LENGTH_PER_TIMES;
	
//	printk("[dm95_spi] len=%d, txbuf=0x%p,rxbuf=0x%p",len,txb,trxb);
	if((len + 1)>CMMB_SPI_INTERFACE_MAX_PKT_LENGTH_PER_TIMES){

#if 0		
        // REMAINDER HAD NO COMMAND!!
        /*
		spi_message_init(&db->spi_dmsg1);
		xfer= &db->spi_dxfer1;
		xfer->tx_buf = txb;
		xfer->rx_buf = trxb;
		xfer->len = RD_LEN_ONE + CMMB_SPI_INTERFACE_MAX_PKT_LENGTH_PER_TIMES*pkt_count;
		spi_message_add_tail(&db->spi_dxfer1, &db->spi_dmsg1);
		
		if (remainder){
			xfer= &db->spi_dxfer2;
			xfer->tx_buf = txb + 1 + (CMMB_SPI_INTERFACE_MAX_PKT_LENGTH_PER_TIMES * pkt_count); //[DUMMY TO EXTEND.JJ.Note]
			xfer->rx_buf = trxb;
			xfer->len = remainder;
			spi_message_add_tail(&db->spi_dxfer2, &db->spi_dmsg1);
		}
		if(INNODev_sync(db))
			printk("[dm95_spi]INNO ERROR: len=%d, txbuf=0x%p,rxbuf=0x%p",len,txb,trxb); //return INNO_GENERAL_ERROR;
        */
#else
        /*
        //xrdbuff_u8=
        xfer = &db->spi_xfer1;	
        xfer->tx_buf = txb;
        xfer->rx_buf = trxb;
        xfer->len = RD_LEN_ONE + len;

		int ret = spi_sync(db->spidev, &db->spi_msg1);
		if (ret < 0){
	    	printk("9051().e out.in.dump_fifo4, %d BYTES, ret=%d\n", len, ret); //"%dth byte", i
			printk(" <failed.e>\n");
		}
        */
		int blkLen= CMMB_SPI_INTERFACE_MAX_PKT_LENGTH_PER_TIMES*pkt_count - 1;
		printk("   9Tx_OvLimit(%d ... ", blkLen);
		printk(")\n");
		
		xfer= &db->spi_dxfer1;
		xfer->tx_buf = txb;
		xfer->rx_buf = trxb;
		xfer->len = RD_LEN_ONE + blkLen; // minus 1, so real all is 1024 * n
		spi_message_init(&db->spi_dmsg1);

		spi_message_add_tail(&db->spi_dxfer1, &db->spi_dmsg1);
		if(INNODev_sync(db))
			printk("[dm95_spi]INNO txERR1: len= %d of %d, txbuf=0x%p,rxbuf=0x%p",blkLen,len,txb,trxb); //return INNO_GENERAL_ERROR;
		
		blkLen= remainder;
		printk("   9Tx_OvRemainder(%d ... ", blkLen);
		printk(")\n");

		/*&txb[CMMB_SPI_INTERFACE_MAX_PKT_LENGTH_PER_TIMES*pkt_count - 1]*/
        wbuff_u8(DM_SPI_WR | DM_SPI_MWCMD, &txb[CMMB_SPI_INTERFACE_MAX_PKT_LENGTH_PER_TIMES*pkt_count - 1]);
		xfer= &db->spi_dxfer1;
		xfer->tx_buf = &txb[CMMB_SPI_INTERFACE_MAX_PKT_LENGTH_PER_TIMES*pkt_count - 1]; // has been minus 1
		xfer->rx_buf = trxb;
		xfer->len = RD_LEN_ONE + remainder; // when calc, it plus 1
		spi_message_init(&db->spi_dmsg1);

		spi_message_add_tail(&db->spi_dxfer1, &db->spi_dmsg1);
		if(INNODev_sync(db))
			printk("[dm95_spi]INNO txERR2: len=%d of %d, txbuf=0x%p,rxbuf=0x%p",blkLen,len,txb,trxb); //return INNO_GENERAL_ERROR;
#endif

	} else {

		spi_message_init(&db->spi_dmsg1);
		xfer= &db->spi_dxfer1;

		xfer->tx_buf = txb;
		xfer->rx_buf = trxb;
		xfer->len = RD_LEN_ONE + len;
		spi_message_add_tail(&db->spi_dxfer1, &db->spi_dmsg1);
		if(INNODev_sync(db))
			printk("[dm95_spi]INNO ERROR: len=%d, txbuf=0x%p,rxbuf=0x%p",len,txb,trxb); //return INNO_GENERAL_ERROR;
	}
}
#endif

/* DM9051 network board routine ---------------------------- */

static void
dm9051_reset(board_info_t * db)
{
	__le16 txb[2];
	  wbuff(DM_SPI_WR| DM9051_NCR | NCR_RST<<8, txb); //org: wrbuff(DM9051_NCR, NCR_RST, txb)
	  xwrbyte(db, txb);
	//=  
	//iiow(db, DM9051_NCR, NCR_RST);
	
	  wbuff(DM_SPI_WR| 0x5e | 0x80<<8, txb); //org: wrbuff(DM9051_NCR, NCR_RST, txb)
	  xwrbyte(db, txb);
	//=  
	//iiow(db, 0x5e, 0x80);
	  printk("iow[%02x %02x].[Set.MBNDRY_STS]\n", 0x5e, 0x80);
	  
	mdelay(1);
}

/*
 *   Write a byte to spi port
 */
static void iiow(board_info_t *db, /*int*/ unsigned reg, /*int*/ unsigned val)
{
	__le16 txb[2];
	wbuff(DM_SPI_WR| reg | val<<8, txb); //org: wrbuff(reg, val, txb)
	xwrbyte(db, txb);
}

static void iow(board_info_t *db, /*int*/ unsigned reg, /*int*/ unsigned val)
{
	__le16 txb[2];

	//iiow(db, reg, val);	
	if (reg!=DM9051_TCR &&reg!=DM9051_TXPLL &&reg!=DM9051_TXPLH
#if DBG_TO_FINAL_ADD_1_INDEED	
	    && reg!=DM9051_ISR &&reg!=DM9051_MWRL &&reg!=DM9051_MWRH
	    &&reg!=DM9000_EPAR &&reg!=DM9000_EPCR
	 /* && reg!=DM9051_FCR */ /* 'DM9000_EPDRL'/'DM9000_EPDRH' by iiow() */
#endif
	)
	  printk("iow[%02X %02X]\n", /*txb[0]&0x7f*/ reg, /*txb[0]>>8*/ val & 0xff); // eg: "iow [7E 82]"
	  // Include: DM9000_GPR, ... etc

	wbuff(DM_SPI_WR| reg | val<<8, txb); //wrbuff(reg, val, txb)
	xwrbyte(db, txb);
}

static u8 iior(board_info_t *db, /*int*/ unsigned reg)
{ 
	__le16 *txb = (__le16 *)db->txd;
	u8 *trxb = db->rxd;
	wbuff(DM_SPI_RD | reg, txb);
	xrdbyte(db, txb, trxb);
	return trxb[1];
}

static u8 ior(board_info_t *db, /*int*/ unsigned reg)
{ 
	__le16 *txb = (__le16 *)db->txd;
	u8 *trxb = db->rxd;
	
	if (reg==DM9051_ISR
	/* x */
	    || reg==DM9051_PAR || reg==(DM9051_PAR+1) || reg==(DM9051_PAR+2)
		|| reg==(DM9051_PAR+3) || reg==(DM9051_PAR+4) || reg==(DM9051_PAR+5)
	/* x */
		|| reg==DM9051_MRRL || reg==DM9051_MRRH
		|| reg==DM_SPI_MRCMDX
#if DBG_TO_FINAL_ADD_1_INDEED	
		|| reg==DM9051_MWRL || reg==DM9051_MWRH
		|| reg==0x22 || reg==0x23
		|| reg==0x24 || reg==0x25
		|| reg==DM9051_FCR || reg==DM9000_EPCR 
		|| reg==DM9000_EPDRH || reg==DM9000_EPDRL
		|| reg==DM9051_TCR
#endif
	)
		return iior(db, reg);
		
	wbuff(DM_SPI_RD | reg, txb);
	printk("51rdreg.MOSI.p: [%02x][..]\n", db->txd[0]);
	xrdbyte(db, txb, trxb);
	printk("51rdreg.MISO.e: [..][%02x]\n", trxb[1]);
	return trxb[1];
}

/* routines for packing to use ior() */

#if 0
static unsigned int dm9051_read_locked(board_info_t *db, int reg)
{
	unsigned long flags;
	unsigned int ret;
	spin_lock_irqsave(&db->statelock, flags);
//#if 1
//#else
//    mutex_lock(&db->addr_lock);
//#endif
	ret = ior(db, reg);
	spin_unlock_irqrestore(&db->statelock, flags);
//#if 1
//#else
//    mutex_unlock(&db->addr_lock);
//#endif
	return ret;
}
#endif

void read_rwr(board_info_t *db, u16 *ptrwr)
{
	*ptrwr= ior(db, 0x24); //v.s. 'DM9051_MRRL'
	*ptrwr |= (u16)ior(db, 0x25) << 8;  //v.s. 'DM9051_MRRH'
}
void read_mrr(board_info_t *db, u16 *ptrmrr)
{
	*ptrmrr= ior(db, DM9051_MRRL);
	*ptrmrr |= (u16)ior(db, DM9051_MRRH) << 8; 
}

void read_tx_rwr(board_info_t *db, u16 *ptrwr)
{
	*ptrwr= ior(db, 0x22); //v.s. 'DM9051_MWRL'
	*ptrwr |= (u16)ior(db, 0x23) << 8;  //v.s. 'DM9051_MWRH'
}
void read_tx_mrr(board_info_t *db, u16 *ptrmrr)
{
	*ptrmrr= ior(db, DM9051_MWRL); // is '0x7A'
	*ptrmrr |= (u16)ior(db, DM9051_MWRH) << 8; // is '0x7B'
}

u16 dm9051_calc(u16 rwregs0, u16 rwregs1)
{
	u32 digiCalc;
	u32 digi, dotdigi;
	
	if (rwregs0>=rwregs1)
		digiCalc= rwregs0 - rwregs1;
	else
		digiCalc= 0x3400 + rwregs0 - rwregs1; //= 0x4000 - rwregs[1] + (rwregs[0] - 0xc00)
		
	digiCalc *= 100;
	digi= digiCalc / 0x3400;
	
	dotdigi= 0;
	digiCalc -= digi * 0x3400;
	if (digiCalc>=0x1a00) dotdigi= 5;
	
	return /*value=*/ ((digi << 8) + dotdigi);
}

u16 dm9051_txcalc(board_info_t *db, u16 rwregs0, u16 rwregs1)
{
	u32 digiCalc;
	u32 digi, dotdigi;
	
	if (rwregs0>=rwregs1)
		digiCalc= rwregs0 - rwregs1;
	else
		digiCalc= 0x0c00 + rwregs0 - rwregs1; 
		
	db->tx_rwregs[0]= rwregs0; //regs[0]; // save in 'tx_xxx'
	db->tx_rwregs[1]= rwregs1; //regs[1];
	
	db->tx_qs= (u16) digiCalc;
		
	digiCalc *= 100;
	digi= digiCalc / 0x0c00;
	
	dotdigi= 0;
	digiCalc -= digi * 0x0c00;
	if (digiCalc>=0x0600) dotdigi= 5;
	
	return /*value=*/ ((digi << 8) + dotdigi);
}

u16 dm9051_add_calc(u16 data_wr_addr, u16 len)
{
	data_wr_addr += len;
	if (data_wr_addr >= 0x0c00)
	  data_wr_addr -= 0x0c00;
	return data_wr_addr;
}

u16 dm9051_rx_cap(board_info_t *db)
{
	u16 rwregs[2];
	read_rwr(db, &rwregs[0]);
	read_mrr(db, &rwregs[1]);
	db->rwregs[0]= rwregs[0]; // save in 'rx_cap'
	db->rwregs[1]= rwregs[1];
	return dm9051_calc(rwregs[0], rwregs[1]);   
}
u16 dm9051_tx_cap(board_info_t *db)
{
	u16 regs[2];
	read_tx_rwr(db, &regs[0]);
	read_tx_mrr(db, &regs[1]);
	return dm9051_txcalc(db, regs[0], regs[1]);  
}

u16 dm9051_rx_cap_lock(board_info_t *db)
{
	u16 ret;
	  mutex_lock(&db->addr_lock);
	  ret= dm9051_rx_cap(db);
	  mutex_unlock(&db->addr_lock);
	  return ret;
}

static void dm9051_fifo_reset(u8 state, u8 *hstr, board_info_t *db)
{		  		 
     if (state==11)
     {
     		if (hstr)
	     	  ++db->bC.FIFO_RST_counter;
	      //printk("%s %d\n", hstr, db->bC.FIFO_RST_counter);
	     	printk("%s Len %d %d\n", hstr, db->bC.RxLen, db->bC.FIFO_RST_counter);
		  	dm9051_reset(db);	
			iiow(db, DM9051_FCR, FCR_FLOW_ENABLE);	/* Flow Control */
			iiow(db, DM9051_PPCR, PPCR_SETTING); /* Fully Pause Packet Count */
	     	iiow(db, DM9051_IMR, IMR_PAR | IMR_PTM | IMR_PRM);
	     	//iiow(db, DM9051_RCR, RCR_DIS_LONG | RCR_DIS_CRC | RCR_RXEN);
	     	  iiow(db, DM9051_RCR, db->rcr_all);
			bcopen_rx_info_clear(&db->bC);
	     	return; 
     }
     if (state==1)
     {
     		u8 pk;
     		if (hstr)
     		{
	     	  ++db->bC.FIFO_RST_counter;
	     	  printk("%s LenNotYeh %d %d\n", hstr, db->bC.RxLen, db->bC.FIFO_RST_counter); //" %d", db->bC.FIFO_RST_counter
	    	}
		  	dm9051_reset(db);
	#if 1
			iiow(db, DM9051_FCR, FCR_FLOW_ENABLE);	/* Flow Control */
			if (hstr)
			  iiow(db, DM9051_PPCR, PPCR_SETTING); /* Fully Pause Packet Count */
			else
			{
			  pk= ior(db, DM9051_PPCR);
			  iow(db, DM9051_PPCR, PPCR_SETTING); /* Fully Pause Packet Count */
			}
	     	iiow(db, DM9051_IMR, IMR_PAR | IMR_PTM | IMR_PRM);
	     	//iiow(db, DM9051_RCR, RCR_DIS_LONG | RCR_DIS_CRC | RCR_RXEN);
	     	  iiow(db, DM9051_RCR, db->rcr_all);
	#endif
			bcopen_rx_info_clear(&db->bC);
	     	return; 
	 }
     if (state==2){
	    printk("------- ---end--- -------\n");
	    printk("\n");
	    printk("Hang.System.JJ..\n");
	    while (1);
     }
     return; 
}

static void dm9051_fifo_reset_statistic(board_info_t *db)
{		
	printk("ERO.Rxb&LargEr\n");
	printk("%d .%d %d\n", 
		db->bC.ERRO_counter, db->bC.RXBErr_counter, db->bC.LARGErr_counter);
	if (db->bC.StatErr_counter)
		printk("StatEr %d\n", db->bC.StatErr_counter);
}

void dm9051_fifo_ERRO(u8 rxbyte, board_info_t *db)
{	
	u16 calc;
	if (db->bC.rxbyte_counter==5 || db->bC.rxbyte_counter0==(NUMRXBYTECOUNTER-1)) {
		 calc= dm9051_rx_cap(db);
	     db->bC.RXBErr_counter++;
	     printk("\n");
	     printk("( Rxb %d ) %d ++ \n", db->bC.RXBErr_counter, db->bC.rxbyte_counter0_to_prt);
	     printk("rxb=%02X rxWrRd .%02x/%02x (RO %d.%d%c)\n", rxbyte, 
	       db->rwregs[0], db->rwregs[1],
	       calc>>8, calc&0xFF, '%');
	     if (!(db->bC.RXBErr_counter%5))
	     {
	       driver_dtxt_disp(db);
	       driver_dloop_disp(db);
	     }
	     dm9051_fifo_reset(1, "dmfifo_reset( RxbEr )", db);
	     dm9051_fifo_reset_statistic(db);
		 printk("\n");
	}
	return;
} 

void dm9051_display_tx_cap(char *hdtitle, board_info_t *db, int item_n, int nTx, u16 txcalc)
{	     
	     if (item_n!=2){ 
	       printk("dm9( %s ) ", hdtitle);
	       printk("txWpRp ./%02x", // (TXQ %d.%d%c)
		       db->tx_rwregs[1]);  // txcalc>>8, txcalc&0xFF, '%',
	     }
	     if (item_n==2){
	       printk("   dm9( %s %d eq %d err %d ) ", hdtitle, nTx, db->tx_eq, db->tx_err);
	       printk("txWpRp .%02x.%d bytes/%02x", // (TXQ %d.%d%c)
		       db->tx_rwregs[0], db->tx_qs, 	// txcalc>>8, txcalc&0xFF, '%',
		       db->tx_rwregs[1]);
	     }
	     printk("\n");
}

void dm9051_display_get_txrwr_sectloop(void)
{
	printk("  ");
}
void dm9051_display_get_txrwr_triploop(board_info_t *db, int nTx, unsigned len)
{
  //.AddCalc= dm9051_add_calc(db->tx_rwregs[1], len);
	u16 regs, AddCalc= dm9051_add_calc(db->tx_rwregs[1], len);
    read_tx_mrr(db, &regs);
    
	if (len==(1514)){
     //.printk("]");
     //.=
        if (regs==AddCalc){
        //printk(" [%d./%02x..", len, regs);
        //printk("E]");
          db->tx_eq++;
        }else{
          printk(" [%d./%02x..", len, regs);
          printk("].ERR(should %02x)", AddCalc);
          db->tx_err++;
        }
	}
    if (len!=(1514)){
     //.printk(")");
     //.=
        if (regs==AddCalc){
        //printk(" (9Tx_ss %d./%02x..", len, regs);
        //printk("E)");
          db->tx_eq++;
        }else{
          printk(" (9Tx_ss %d./%02x..", len, regs);
          printk(").ERR(should %02x)", AddCalc);
          db->tx_err++;
        }
    }
      
    /* Next 'rwregs[1]' */
    db->tx_rwregs[1]= regs;
    
  #if 0  
    //if (!(nTx%5)) {
    //    printk("\n");
	//	dm9051_display_get_txrwr_sectloop();
    //}
  #endif
}
void dm9051_display_get_txrwr_endloop(int nTx)
{
    printk("\n");
	//if (nTx==1) printk(" %d pkt\n", nTx);
	//else printk(" %d pkts\n", nTx);
}

/* routines for sending block to chip */
void dwrite_1024_Limitation(board_info_t *db, u8 *txb, u8 *trxb, int len)
{
	int blkLen;
	struct spi_transfer *xfer;
	int const pkt_count = (len + 1)/ CMMB_SPI_INTERFACE_MAX_PKT_LENGTH_PER_TIMES;
	int const remainder = (len + 1)% CMMB_SPI_INTERFACE_MAX_PKT_LENGTH_PER_TIMES;
	
	if((len + 1)>CMMB_SPI_INTERFACE_MAX_PKT_LENGTH_PER_TIMES){
	
	blkLen= CMMB_SPI_INTERFACE_MAX_PKT_LENGTH_PER_TIMES*pkt_count - 1;
        printk("9Tx tbf=0x%p,rbf=0x%p)\n", db->tmpTxPtr, db->tmpRxPtr);
		
	wbuff_u8(DM_SPI_WR | DM_SPI_MWCMD, db->tmpTxPtr); //'RD_LEN_ONE'
        xfer= &db->spi_dxfer1;

#if 1
      //memcpy(db->tmpTxPtr, txb, RD_LEN_ONE + blkLen);
        memcpy(db->tmpTxPtr+1, txb, blkLen);
		xfer->tx_buf = db->tmpTxPtr; //txb;
		xfer->rx_buf = db->tmpRxPtr; //NULL; //tmpRxPtr; //trxb; ((( When DMA 'NULL' is not good~~~
#else
#endif

		xfer->len = RD_LEN_ONE + blkLen; // minus 1, so real all is 1024 * n
		spi_message_init(&db->spi_dmsg1);

		spi_message_add_tail(&db->spi_dxfer1, &db->spi_dmsg1);
		if(INNODev_sync(db))
			printk("[dm95_spi]INNO txERR1: len= %d of %d, txbuf=0x%p,rxbuf=0x%p",blkLen,len,xfer->tx_buf,xfer->rx_buf); //return INNO_GENERAL_ERROR;

		//(2)	
		blkLen= remainder;
	  //printk("   9Tx_Rem(%d ... ", remainder); printk("tbf=0x%p,rbf=0x%p)", db->tmpTxPtr, db->tmpRxPtr); printk("\n");
		
		xfer= &db->spi_dxfer1;
#if 1

                memcpy(db->tmpTxPtr+1, &txb[CMMB_SPI_INTERFACE_MAX_PKT_LENGTH_PER_TIMES*pkt_count - 1], remainder);
		xfer->tx_buf = db->tmpTxPtr; //&txb[CMMB_SPI_INTERFACE_MAX_PKT_LENGTH_PER_TIMES*pkt_count - 1]; // has been minus 1
		xfer->rx_buf = db->tmpRxPtr; //NULL; //tmpRxPtr; //trxb; (((  'NULL' is not good~~~
#else
#endif

		xfer->len = RD_LEN_ONE + remainder; // when calc, it plus 1
		spi_message_init(&db->spi_dmsg1);

		spi_message_add_tail(&db->spi_dxfer1, &db->spi_dmsg1);
		if(INNODev_sync(db))
			printk("[dm95_spi]INNO txERR2: len=%d of %d, txbuf=0x%p,rxbuf=0x%p",blkLen,len,xfer->tx_buf,xfer->rx_buf); //return INNO_GENERAL_ERROR;
	} else {
	  //printk("   9Tx_Sma_(%d ... ", len); printk("tbf=0x%p,rbf=0x%p)", db->tmpTxPtr, db->tmpRxPtr); printk("\n");
	  //wbuff_u8(DM_SPI_WR | DM_SPI_MWCMD, txb);
		wbuff_u8(DM_SPI_WR | DM_SPI_MWCMD, db->tmpTxPtr);
		spi_message_init(&db->spi_dmsg1);
		xfer= &db->spi_dxfer1;
#if 1
      //memcpy(db->tmpTxPtr, txb, RD_LEN_ONE + len);
        memcpy(db->tmpTxPtr+1, txb, len);
		xfer->tx_buf = db->tmpTxPtr; //txb;
		xfer->rx_buf = db->tmpRxPtr; //NULL; //tmpRxPtr; //trxb; ((( again When DMA 'NULL' is not good~~~
#endif
		xfer->len = RD_LEN_ONE + len;
		spi_message_add_tail(&db->spi_dxfer1, &db->spi_dmsg1);
		if(INNODev_sync(db))
	           printk("[dm95_spi]INNO ERROR: len=%d, txbuf=0x%p,rxbuf=0x%p",len,xfer->tx_buf,xfer->rx_buf); //return INNO_GENERAL_ERROR;

	}
}

void dwrite_32_Limitation(board_info_t *db, u8 *txb, u8 *trxb, int len)
{
  int n;
  unsigned counter, not_process;
  
  not_process= len;
  for (n= 0; n< len; n += 31){	// n += 'counter' is the exact calc, But we use n += 31 can get the same looping..	
  		if (not_process <= 31)
  		  counter= not_process;
  		else 
  		  counter= 31;
  		//counter++; // counter= 32; or less
  		//But: //counter= 31; or less
		wbuff_u8(DM_SPI_WR | DM_SPI_MWCMD, txb);
		xrdbuff_u8(db, txb, NULL, counter);
		not_process -= counter;
		txb += counter;
  }
}

void dwrite_1_Limitation(board_info_t *db, u8 *txb, u8 *trxb, int len)
{
  int n;
  for (n=0; n< len; n++){
		wbuff_u8(DM_SPI_WR | DM_SPI_MWCMD, txb);
		xrdbuff_u8(db, txb, NULL, 1);
		txb++;
  }
}

#if DM9051_CONF_TX
static void dm9051_outblk(board_info_t *db, u8 *buff, unsigned len) //(txp->data, tx_len)
{
	//WHY?
	//SYNC_len= ALIGN(len, 4); // [DA got problem! Think about the chip can not synchronous for TX DATA BYTES]
	//int SYNC_len= len;
	if (len>DM9051_PKT_MAX) printk("[WARN9051: Large TX packet is present!!!\n");

#if DMA3_P2_TSEL_1024F
	dwrite_1024_Limitation(db, buff, NULL, len);
	
#elif DMA3_P2_TSEL_32F
        memcpy(&db->TxDatBuf[1], buff, len);
        dwrite_32_Limitation(db, db->TxDatBuf, NULL, len);

#elif DMA3_P2_TSEL_1F
	memcpy(&db->TxDatBuf[1], buff, len);
	dwrite_1_Limitation(db, db->TxDatBuf, NULL, len);
	
#else
	memcpy(&db->TxDatBuf[1], buff, len);
	wbuff_u8(DM_SPI_WR | DM_SPI_MWCMD, db->TxDatBuf);
	xrdbuff_u8(db, db->TxDatBuf, NULL, len);
#endif  
}
#endif

void dread_1024_Limitation(board_info_t *db, u8 *trxb, int len)
{
	struct spi_transfer *xfer;
	u8 txb[1];
	int const pkt_count = (len + 1) / CMMB_SPI_INTERFACE_MAX_PKT_LENGTH_PER_TIMES;
	int const remainder = (len + 1) % CMMB_SPI_INTERFACE_MAX_PKT_LENGTH_PER_TIMES;
	if((len + 1)>CMMB_SPI_INTERFACE_MAX_PKT_LENGTH_PER_TIMES){	
		int blkLen;
		wbuff_u8(DM_SPI_RD | DM_SPI_MRCMD, txb);
		//(1)
		blkLen= CMMB_SPI_INTERFACE_MAX_PKT_LENGTH_PER_TIMES*pkt_count - 1;

	    printkr("dm9rx_EvenPar_OvLimit(%d ... ", blkLen);
	    printkr("txbf=0x%p,rxbf=0x%p)\n", db->tmpTxPtr, db->tmpRxPtr);
    
		spi_message_init(&db->spi_dmsg1);
		xfer= &db->spi_dxfer1;
        memcpy(db->tmpTxPtr, txb, 2);
        //memcpy(db->tmpRxPtr, trxb, RD_LEN_ONE + (CMMB_SPI_INTERFACE_MAX_PKT_LENGTH_PER_TIMES*pkt_count) - 1);
		xfer->tx_buf = db->tmpTxPtr; //txb;
		xfer->rx_buf = db->tmpRxPtr; //trxb;
		xfer->len = RD_LEN_ONE + (CMMB_SPI_INTERFACE_MAX_PKT_LENGTH_PER_TIMES*pkt_count) - 1;  // minus 1, so real all is 1024 * n
		spi_message_add_tail(&db->spi_dxfer1, &db->spi_dmsg1);
		if(INNODev_sync(db/*&db->spi_dmsg1*/))
			printk("[dm95_spi]INNO1 ERROR: len=%d, txbuf=0x%p,rxbuf=0x%p",len,txb,trxb); //return INNO_GENERAL_ERROR;
        memcpy(trxb, db->tmpRxPtr, RD_LEN_ONE + (CMMB_SPI_INTERFACE_MAX_PKT_LENGTH_PER_TIMES*pkt_count) - 1);
		//(2)	
		blkLen= remainder;
		printkr("dm9rx_EvenPar_OvRemainder(%d ... ", blkLen);
		printkr("txbf=0x%p,rxbf=0x%p)\n", db->tmpTxPtr, db->tmpRxPtr);

		spi_message_init(&db->spi_dmsg1);
		xfer= &db->spi_dxfer1;
      //memcpy(db->tmpTxPtr, txb, 2);
      //memcpy(db->tmpRxPtr, db->TxDatBuf, RD_LEN_ONE + remainder);
		xfer->tx_buf = db->tmpTxPtr; //txb;
		xfer->rx_buf = db->tmpRxPtr; //db->TxDatBuf;
		xfer->len = RD_LEN_ONE + remainder; // when calc, it plus 1
		spi_message_add_tail(&db->spi_dxfer1, &db->spi_dmsg1);
		if(INNODev_sync(db/*&db->spi_dmsg1*/))
			printk("[dm95_spi]INNO2 ERROR: len=%d, txbuf=0x%p,rxbuf=0x%p",len,txb,xfer->rx_buf); //return INNO_GENERAL_ERROR;

        memcpy(trxb + (CMMB_SPI_INTERFACE_MAX_PKT_LENGTH_PER_TIMES*pkt_count), &db->tmpRxPtr[1], remainder);
	}
	else{
		printkr("dm9rx_smal_(%d ... ", len);
		printkr("txbf=0x%p,rxbf=0x%p)\n", db->tmpTxPtr, db->tmpRxPtr);

		wbuff_u8(DM_SPI_RD | DM_SPI_MRCMD, txb);
		spi_message_init(&db->spi_dmsg1);
		xfer= &db->spi_dxfer1;

#if 1
	  wbuff_u8(DM_SPI_RD | DM_SPI_MRCMD, db->tmpTxPtr);
#else
         memcpy(db->tmpTxPtr, txb, 2);
        //memcpy(db->tmpRxPtr, 
#endif
		xfer->tx_buf = db->tmpTxPtr; //txb;
		xfer->rx_buf = db->tmpRxPtr; //trxb;
		xfer->len = RD_LEN_ONE + len;
		spi_message_add_tail(&db->spi_dxfer1, &db->spi_dmsg1);
		if(INNODev_sync(db))
			printk("[dm95_spi]INNO ERROR: len=%d, txbuf=0x%p,rxbuf=0x%p",len,txb,trxb); //return INNO_GENERAL_ERROR;

		printkr("dm9rx_smal_tx_cmd(%x) ... \n", db->tmpTxPtr[0]);

		memcpy(trxb, db->tmpRxPtr, RD_LEN_ONE + len);                
		//dread_32_Limitation(db, trxb, len);
	}
} //printkr

void dread_32_Limitation(board_info_t *db, u8 *trxb, int len)
{
  u8 txb[1];
  
  int n;
  unsigned counter, not_process;
  
  wbuff_u8(DM_SPI_RD | DM_SPI_MRCMD, txb);
  trxb++;
  
  not_process= len;
  for (n= 0; n< len; n += 31){	// n += 'counter' is the exact calc, But we use n += 31 can get the same looping..	
	if (not_process <= 31)
	  counter= not_process;
	else 
	  counter= 31;
  	
  	xrdbuff_u8(db, txb, db->TxDatBuf, counter);
  	memcpy(trxb + n, &db->TxDatBuf[1], counter);
  	not_process -= counter;
  }
}

void dread_1_Limitation(board_info_t *db, u8 *trxb, int len)
{
  int n;
  u8 txb[1];
  u8 test_buf[6]; //len is 4
  wbuff_u8(DM_SPI_RD | DM_SPI_MRCMD, txb);
  trxb++;
  for (n=0; n< len; n++){
  	xrdbuff_u8(db, txb, test_buf, 1);
  	*trxb++= test_buf[1]; 
  }
}
		
static void dm9051_inblk(board_info_t *db, u8 *buff, unsigned len)
{
	// Note: Read into &buff[1]...
	u8 txb[1];
	
	if (len==1){
		wbuff_u8(DM_SPI_RD | DM_SPI_MRCMD, txb);
		xrdbuff_u8(db, txb, buff, 1);
		return;
	}
	
#if DMA3_P2_RSEL_1024F
	dread_1024_Limitation(db, buff, (int)len);
#elif DMA3_P2_RSEL_32F
	dread_32_Limitation(db, buff, (int)len);
#elif DMA3_P2_RSEL_1F
	dread_1_Limitation(db, buff, (int)len);
#else
	wbuff_u8(DM_SPI_RD | DM_SPI_MRCMD, txb);
	xrdbuff_u8(db, txb, buff, len);
	return;
#endif
}
static void dm9051_dumpblk(board_info_t *db, unsigned len)
{
	unsigned i;
	u8 rxb[2]; // Note: One shift...
	u8 txb[1];
	wbuff_u8(DM_SPI_RD | DM_SPI_MRCMD, txb);
	for(i=0; i<len; i++)
	  xrdbuff_u8(db, txb, rxb, 1);
}

/* routines for packing to use read/write blk */

static void dm9051_rd_rxhdr(board_info_t *db, u8 *buff, unsigned len)
{	
	u8 test_buf[12]; //len is 4
	//dm9051_inblk(db, test_buf, len);
	//memcpy(buff, test_buf + 1, len); 
	
	dm9051_inblk(db, test_buf, 1);
	buff[0]= test_buf[1];
	dm9051_inblk(db, test_buf, 1);
	buff[1]= test_buf[1];
	dm9051_inblk(db, test_buf, 1);
	buff[2]= test_buf[1];
	dm9051_inblk(db, test_buf, 1);
	buff[3]= test_buf[1];
}

#if 0
static void dm9051_rd_rxdata(board_info_t *db, u8 *pre1_buff, int len)
{
    
    #if 1
	//(Even quantity bytes!?)
 	//(or= Last byte)
    // Because: One byte shift...
    #endif
    #if 1
    /*
    unsigned EvenLen;
	EvenLen= len & ~0x1;
	dm9051_inblk(db, pre1_buff, EvenLen);
	if (EvenLen==len)  
		return;
		
 	//(or= Last byte)
    cnt= (int) len;
    dm9051_inblk(db, pre1_buff + cnt, 1);
	pre1_buff[cnt]= pre1_buff[cnt+1];
    */
	dm9051_inblk(db, pre1_buff, len);
    #endif
    
    #if 0
    int cnt;
	u8 test_buf[6]; //len is 4
	
	u8 txb[1];
	wbuff_u8(DM_SPI_RD | DM_SPI_MRCMD, txb);
    for (cnt=1; cnt<=len; cnt++)
    {
	  //dm9051_inblk(db, test_buf, 1);
	  //=
	  xrdbuff_u8(db, txb, test_buf, 1);
	  pre1_buff[cnt]= test_buf[1];
    }
    #endif
}  
#endif    

static void dm9051_disp_hdr_s(board_info_t *db)
{
	u16 calc= dm9051_rx_cap(db);
	//printk("dm9.hdrRd.s.%02x/%02x(RO %d.%d%c)\n", db->rwregs[0], db->rwregs[1], calc>>8, calc&0xFF, '%'); //hdrWrRd
	db->DERFER_rwregs[0]= db->rwregs[0];
	db->DERFER_rwregs[1]= db->rwregs[1];
	db->DERFER_calc= calc;
}
static void dm9051_disp_hdr_e(board_info_t *db, int rxlen)
{
	//u16 calc= dm9051_rx_cap(db);
	//printk("hdrWrRd.e.%02x/%02x(RO %d.%d%c) rxLen(0x%x)= %d\n", db->rwregs[0], db->rwregs[1], calc>>8, calc&0xFF, '%', 
	//  rxlen, rxlen);
}
static bool dm9051_chk_data(board_info_t *db, u8 *rdptr, int RxLen)
{
	struct net_device *dev = db->ndev;
	u16 calc;
#if 1
    if (rdptr[12]==0x08 && rdptr[13]==0x00)
      ; // Not to log, This is IP packet data
    else
    if (RxLen!=(1518)){ // (1514+4)
    //Display Debug RX-pkt	
    //printk("[RX.found.s]\n");
     calc= dm9051_rx_cap(db);
    //printk("dm9.hdrRd.s.%02x/%02x(RO %d.%d%c)\n", db->DERFER_rwregs[0], db->DERFER_rwregs[1], DERFER_calc>>8, DERFER_calc&0xFF, '%');
     if (RxLen >= (CMMB_SPI_INTERFACE_MAX_PKT_LENGTH_PER_TIMES-1))
      printkr(" OvLen=%d.", RxLen);
     else
      printkr(" SmallLen=%d.", RxLen);
     printkr("dm9rx.e.%02x/%02x(RO %d.%d%c) ", db->rwregs[0], db->rwregs[1], calc>>8, calc&0xFF, '%'); //dataWrRd
     printkr("rx(%02x %02x %02x %02x %02x. ", rdptr[0], rdptr[1],rdptr[2],rdptr[3],rdptr[4]); //"%02x %02x %02x" ,rdptr[3],rdptr[4],rdptr[5]
     printkr("%02x %02x .. ", rdptr[6], rdptr[7]);
     printkr("%02x %02x\n", rdptr[12],rdptr[13]);
    }
#endif 

#if 0	
	u16 sum_u6,sum_v6;	
 		//u8 *skbdata; //= rdptr
		sum_u6= rdptr[0]+rdptr[1]+rdptr[2]+rdptr[3]+rdptr[4]+rdptr[5];
		sum_v6= dev->dev_addr[1]+dev->dev_addr[2]+dev->dev_addr[3]+dev->dev_addr[4]+dev->dev_addr[5]+dev->dev_addr[6]; //BUG [1~6],NEED [0~5]
		
		if (sum_u6==(0xff+0xff+0xff+0xff+0xff+0xff)) // 0x5FA
		   db->bC.rx_brdcst_counter++;
		if (rdptr[0]&1) //'skb->data[0]'
			db->bC.rx_multi_counter++;
			
		//disp..change_location..			        
        if (sum_u6!=(0xff+0xff+0xff+0xff+0xff+0xff))
        {
          if (sum_u6==sum_v6)
          	db->bC.rx_unicst_counter++;
          else if (rdptr[0]&1) //'skb->data[0]'
			; //read_mrr(db, &ckRXcurr);
          else
          {
            //"[ERRO.found.s]"
          }
        }
#endif
	if (rdptr[0]!=dev->dev_addr[0] || rdptr[1]!=dev->dev_addr[1] || rdptr[2]!=dev->dev_addr[2])
	{
		if (rdptr[0]&1) //'skb->data[0]'
		{
		 	#if 0
			db->bC.rx_multi_counter++;
		 	#endif
            return true;
		}
		else
		{
            //"[ERRO.found.s]"
            calc= dm9051_rx_cap(db);
            printk("\n");
            printk("rWrRd.%02x/%02x(RO %d.%d%c) ", db->rwregs[0], db->rwregs[1], calc>>8, calc&0xFF, '%');
            printk("fifo_rst(.s %02x %02x %02x %02x %02x %02x\n", rdptr[0], rdptr[1],rdptr[2],rdptr[3],rdptr[4],rdptr[5]);
            calc= dm9051_rx_cap(db);
            printk("rWrRd.%02x/%02x(RO %d.%d%c) ", db->rwregs[0], db->rwregs[1], calc>>8, calc&0xFF, '%');
            printk("fifo_should %02x %02x %02x %02x %02x %02x\n", dev->dev_addr[0], dev->dev_addr[1],dev->dev_addr[2],
              dev->dev_addr[3],dev->dev_addr[4],dev->dev_addr[5]);
			db->bC.ERRO_counter++;
			printk("( ERO %d ) %d ++ \n", db->bC.ERRO_counter, db->bC.rxbyte_counter0_to_prt);
			dm9051_fifo_reset(11, "dmfifo_reset( 11 )", db); //~= dm9051_fifo_reset(11, ...)
            calc= dm9051_rx_cap(db);
            printk("rWrRd.%02x/%02x(RO %d.%d%c) ", db->rwregs[0], db->rwregs[1], calc>>8, calc&0xFF, '%');
            printk("fifo_rst(.e %02x %02x %02x %02x %02x %02x\n", rdptr[0], rdptr[1],rdptr[2],rdptr[3],rdptr[4],rdptr[5]);
            dm9051_fifo_reset_statistic(db);
			printk("\n");
            return false;
		}
	}
	db->bC.rx_unicst_counter++;
    return true;
}   
  
static void dm_schedule_phy(board_info_t *db)
{  
  //schedule_delayed_work(&db->phy_poll, HZ * 2); 3 seconds instead
	schedule_delayed_work(&db->phy_poll, HZ * 3);
}

static int dm9051_write_mac_addr(struct net_device *dev)
{
	board_info_t *db = netdev_priv(dev);
	int i;

	for (i = 0; i < ETH_ALEN; i++)
	  iow(db, DM9051_PAR+i, dev->dev_addr[i]);

	return 0;
}

static void dm9051_init_mac(board_info_t *db)
{
	struct net_device *dev = db->ndev;

	random_ether_addr(dev->dev_addr);
#if 1
	dev->dev_addr[0]= 0x00;
	dev->dev_addr[1]= 0xff;
	dev->dev_addr[2]= 0x00;
	dev->dev_addr[3]= 0x00;
	dev->dev_addr[4]= 0x90;
	dev->dev_addr[5]= 0x51;
#endif
	printk("[dm9051.init_mac() %02X %02X %02X  %02X %02X %02X\n", dev->dev_addr[0],
	  dev->dev_addr[1], dev->dev_addr[2], dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5]);

	mutex_lock(&db->addr_lock);
	dm9051_write_mac_addr(dev);
	mutex_unlock(&db->addr_lock);
}

#if 0
static int dm9000_wait_eeprom(board_info_t *db)
{
}
#endif

#if 1
/*
 *  Read a word data from EEPROM
 */
/*
	Caller's example:
	u8 tmp[2];
	dm9051_read_eeprom(db, i, tmp); // i=0, 1, 2, ... 63
 */
static void
dm9051_read_eeprom(board_info_t *db, int offset, u8 *to)
{
	mutex_lock(&db->addr_lock);

	//unsigned long flags;	
	//spin_lock_irqsave(&db->statelock, flags);
	iiow(db, DM9000_EPAR, offset);
	iiow(db, DM9000_EPCR, EPCR_ERPRR);
	//spin_unlock_irqrestore(&db->statelock, flags);
	
	//dm9051_msleep(db, 1);		/* Wait read complete */
	//= 
	while ( iior(db, DM9000_EPCR) & EPCR_ERRE) ;
	
	//spin_lock_irqsave(&db->statelock, flags);
	iiow(db, DM9000_EPCR, 0x0);	
	to[0] = iior(db, DM9000_EPDRL);
	to[1] = iior(db, DM9000_EPDRH);
	//spin_unlock_irqrestore(&db->statelock, flags);
	
	mutex_unlock(&db->addr_lock);
}

/*
 * Write a word data to SROM
 */
static void
dm9051_write_eeprom(board_info_t *db, int offset, u8 *data)
{
	mutex_lock(&db->addr_lock);
	
	iiow(db, DM9000_EPAR, offset);
	iiow(db, DM9000_EPDRH, data[1]);
	iiow(db, DM9000_EPDRL, data[0]);
	iiow(db, DM9000_EPCR, EPCR_WEP | EPCR_ERPRW);
	
	//dm9051_msleep(db, 1);		/* Wait read complete */
	//= 
	while ( iior(db, DM9000_EPCR) & EPCR_ERRE) ;
	
	iow(db, DM9000_EPCR, 0);
	
	mutex_unlock(&db->addr_lock);
}
#endif

#define DM9051_PHY		0x40	/* PHY address 0x01 */

//SPI:
// do before: mutex_lock(&db->addr_lock); | (spin_lock_irqsave(&db->statelock,flags);)
// do mid: spin_unlock_irqrestore(&db->statelock,flags);, spin_lock_irqsave(&db->statelock,flags);
// do after: (spin_unlock_irqrestore(&db->statelock,flags);) | mutex_unlock(&db->addr_lock);
static int dm9051_phy_read(struct net_device *dev, int phy_reg_unused, int reg)
{
	board_info_t *db = netdev_priv(dev);
	int ret;

	/* Fill the phyxcer register into REG_0C */
	iiow(db, DM9000_EPAR, DM9051_PHY | reg);
	iiow(db, DM9000_EPCR, EPCR_ERPRR | EPCR_EPOS);	/* Issue phyxcer read command */

	//dm9051_msleep(db, 1);		/* Wait read complete */
	//= 
	while ( ior(db, DM9000_EPCR) & EPCR_ERRE) ;

	iiow(db, DM9000_EPCR, 0x0);	/* Clear phyxcer read command */
	/* The read data keeps on REG_0D & REG_0E */
	ret = (ior(db, DM9000_EPDRH) << 8) | ior(db, DM9000_EPDRL);
	return ret;
}

static void dm9051_phy_write(struct net_device *dev,
		 int phyaddr_unused, int reg, int value)
{
	board_info_t *db = netdev_priv(dev);

	printk("iowPHY[%02d %04x]\n", reg, value);
	/* Fill the phyxcer register into REG_0C */
	iow(db, DM9000_EPAR, DM9051_PHY | reg);
	/* Fill the written data into REG_0D & REG_0E */
	iiow(db, DM9000_EPDRL, value);
	iiow(db, DM9000_EPDRH, value >> 8);
	iow(db, DM9000_EPCR, EPCR_EPOS | EPCR_ERPRW);	/* Issue phyxcer write command */

	//dm9051_msleep(db, 1);		/* Wait write complete */
	//= 
	while ( ior(db, DM9000_EPCR) & EPCR_ERRE) ;

	iow(db, DM9000_EPCR, 0x0);	/* Clear phyxcer write command */
}

/*
 * Initialize dm9051 board
 */
static void dm9051_init_dm9051(struct net_device *dev)
{
	board_info_t *db = netdev_priv(dev);
	int	phy4;

	iiow(db, DM9051_GPCR, GPCR_GEP_CNTL);	/* Let GPIO0 output */
	
	/* dm9051_reset(db); */

/* DBG_20140407 */
  phy4= dm9051_phy_read(dev, 0, MII_ADVERTISE);	
  dm9051_phy_write(dev, 0, MII_ADVERTISE, phy4 | ADVERTISE_PAUSE_CAP);	/* dm95 flow-control RX! */	
  dm9051_phy_read(dev, 0, MII_ADVERTISE);

	/* Program operating register */
	iow(db, DM9000_TCR, 0);	        /* TX Polling clear */
	iiow(db, DM9000_BPTR, 0x3f);	/* Less 3Kb, 200us */
	iiow(db, DM9000_SMCR, 0);        /* Special Mode */
	/* clear TX status */
	iiow(db, DM9051_NSR, NSR_WAKEST | NSR_TX2END | NSR_TX1END);
	iow(db, DM9051_ISR, ISR_CLR_STATUS); /* Clear interrupt status */

	/* Init Driver variable */
	db->imr_all = IMR_PAR | IMR_PTM | IMR_PRM;
	db->rcr_all= RCR_DIS_LONG | RCR_DIS_CRC | RCR_RXEN;

	/*
	 * (Set address filter table) 
	 * After.call.ndo_open
	 * "kernel_call.ndo_set_multicast_list.later".
	 *   'dm9000_hash_table_unlocked'(dev);
	*/
    //(1)
    dm9051_fifo_reset(1, NULL, db); // 'NULL' for reset FIFO, and no increase the RST counter
}

/* ethtool ops */

static void dm9051_get_drvinfo(struct net_device *dev,
			       struct ethtool_drvinfo *info)
{
	strcpy(info->driver, CARDNAME_9051);
	strcpy(info->version, DRV_VERSION);
	strlcpy(info->bus_info, dev_name(dev->dev.parent), sizeof(info->bus_info));
}

static int dm9000_get_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	board_info_t *dm = to_dm9051_board(dev);
	mii_ethtool_gset(&dm->mii, cmd);
	return 0;
}

static int dm9000_set_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	board_info_t *dm = to_dm9051_board(dev);
	return mii_ethtool_sset(&dm->mii, cmd);
}

static u32 dm9000_get_msglevel(struct net_device *dev)
{
	board_info_t *dm = to_dm9051_board(dev);
	return dm->msg_enable;
}

static void dm9000_set_msglevel(struct net_device *dev, u32 value)
{
	board_info_t *dm = to_dm9051_board(dev);
	dm->msg_enable = value;
}

static int dm9000_nway_reset(struct net_device *dev)
{
	board_info_t *dm = to_dm9051_board(dev);
	return mii_nway_restart(&dm->mii);
}

static u32 dm9000_get_link(struct net_device *dev)
{
	board_info_t *dm = to_dm9051_board(dev);
	return (u32)dm->link;
}

#define DM_EEPROM_MAGIC		(0x444D394B)

static int dm9000_get_eeprom_len(struct net_device *dev)
{
	return 128;
}

static int dm9000_get_eeprom(struct net_device *dev,
			     struct ethtool_eeprom *ee, u8 *data)
{
	board_info_t *dm = to_dm9051_board(dev);
	int offset = ee->offset;
	int len = ee->len;
	int i;

	/* EEPROM access is aligned to two bytes */
	if ((len & 1) != 0 || (offset & 1) != 0)
		return -EINVAL;

	ee->magic = DM_EEPROM_MAGIC;

	for (i = 0; i < len; i += 2)
		dm9051_read_eeprom(dm, (offset + i) / 2, data + i);
	return 0;
}

static int dm9000_set_eeprom(struct net_device *dev,
			     struct ethtool_eeprom *ee, u8 *data)
{
	board_info_t *dm = to_dm9051_board(dev);
	int offset = ee->offset;
	int len = ee->len;
	int i;

	/* EEPROM access is aligned to two bytes */
	if ((len & 1) != 0 || (offset & 1) != 0)
		return -EINVAL;

	if (ee->magic != DM_EEPROM_MAGIC)
		return -EINVAL;

	for (i = 0; i < len; i += 2)
		dm9051_write_eeprom(dm, (offset + i) / 2, data + i);
	return 0;
}

static const struct ethtool_ops dm9051_ethtool_ops = {
	.get_drvinfo		= dm9051_get_drvinfo,
	.get_settings		= dm9000_get_settings,
	.set_settings		= dm9000_set_settings,
	.get_msglevel		= dm9000_get_msglevel,
	.set_msglevel		= dm9000_set_msglevel,
	.nway_reset			= dm9000_nway_reset,
	.get_link			= dm9000_get_link,
 	.get_eeprom_len		= dm9000_get_eeprom_len,
 	.get_eeprom			= dm9000_get_eeprom,
 	.set_eeprom			= dm9000_set_eeprom,
};

#if 1
//phy
static unsigned int dm9051_read_mutex(board_info_t *db, int reg)
{
	unsigned int ret;
    mutex_lock(&db->addr_lock);
	ret = iior(db, reg);
    mutex_unlock(&db->addr_lock);
	return ret;
}
static void dm9051_show_carrier(board_info_t *db,
				unsigned carrier, unsigned nsr)
{
	struct net_device *ndev = db->ndev;
	unsigned ncr = dm9051_read_mutex(db, DM9051_NCR);

	if (carrier)
	  //dev_info(&db->spidev->dev, "%s: link up, %dMbps, %s-duplex, no LPA\n",
	  //	 ndev->name, (nsr & NSR_SPEED) ? 10 : 100,
	  //	 (ncr & NCR_FDX) ? "full" : "half");
		printk("%s: link up, %dMbps, %s-duplex, no LPA\n",
			 ndev->name, (nsr & NSR_SPEED) ? 10 : 100,
			 (ncr & NCR_FDX) ? "full" : "half");
	else
	  //dev_info(&db->spidev->dev, "%s: link down\n", ndev->name);
		printk("%s: link down\n", ndev->name);
}

static void 
dm_poll_phy(struct work_struct *w)
{ 
	struct delayed_work *dw = to_delayed_work(w);
	board_info_t *db = container_of(dw, board_info_t, phy_poll);
	struct net_device *dev = db->ndev;
	unsigned old_carrier = netif_carrier_ok(dev) ? 1 : 0;
	unsigned new_carrier;
	unsigned nsr; 

    nsr= dm9051_read_mutex(db, DM9051_NSR);
	new_carrier= (nsr & NSR_LINKST) ? 1 : 0;

#if 1
    if (new_carrier)
    	;
    else
    {
    	dm9051_show_carrier(db, new_carrier, nsr); // Added more here!! 'nsr' no-used in it!
		printk("ior[%02x][%02x]\n", DM9051_NSR, nsr);
		
	#if 0
		//function: dm9051_software_auto(void) {}
		int	phy20;
    	mutex_lock(&db->addr_lock);
		phy20= dm9051_phy_read(dev, 0, 20);
		
		//. if (!(phy20&0x10)) // HP Auto-MDIX is Enable
		//. {
			  if (phy20&0x20) //MDIX //OR &0x80
			  {
				printk("( dm_poll_phy nsr=0x%02x phy20= 0x%04x MDIX down) %d ++ \n", 
				  nsr, phy20, db->bC.rxbyte_counter0_to_prt);
			    printk("( dm_poll_phy WR phy20= 0x%04x [chg to MDI])\n", 0x10);
			    dm9051_phy_write(dev, 0, 20, 0x10);
			  }
			  else // if () MDI
			  {
				printk("( dm_poll_phy nsr=0x%02x phy20= 0x%04x MDI down) %d ++ \n", 
				  nsr, phy20, db->bC.rxbyte_counter0_to_prt);
			    printk("( dm_poll_phy WR phy20= 0x%04x [chg to MDIX])\n", 0x30);
			    dm9051_phy_write(dev, 0, 20, 0x30);
			  }
		//. }
		mutex_unlock(&db->addr_lock);
	#endif
	}
#endif	
	
	//{
	//unsigned nsr = dm9000_read_locked(db, DM9000_NSR);
	//new_carrier = .
	if (old_carrier != new_carrier) {
	    if (new_carrier) //(netif_msg_link(db)), use 'new_carrier' is better!!
			dm9051_show_carrier(db, new_carrier, nsr);

		if (!new_carrier)
			netif_carrier_off(dev);
		else
			netif_carrier_on(dev);
	}
	//}

	if (netif_running(dev))
	  dm_schedule_phy(db);
}
#endif 

//--------------------------------------------------------------------------------------
/* routines for dbg tasks handling */
//--------------------------------------------------------------------------------------
/* routines for logic tasks handling */
//--------------------------------------------------------------------------------------
/* ethtool support */
//--------------------------------------------------------------------------------------

/**
 * schwrk_loop_xmit - empty
 * schwrk_loop_xmit_inRx - process tx packet(s)
 * @work: The work strucutre what was scheduled.
 *
 * This is called when a packet has been scheduled for
 * transmission and need to be sent to the device.
 */

 	/* LOOP_XMIT & SCH_XMIT */

#if DM9051_CONF_TX 	
static void opening_wake_queue1(struct net_device *dev) //( u8 flag)
{
	board_info_t *db= netdev_priv(dev);
	if (db->bt.prob_cntStopped)
	{
		db->bt.prob_cntStopped= 0;
		netif_wake_queue(dev);
	}
	
	#if 0	
    //No need display & no need wake it up.	
    /*
	else
	{
        //Old version: dbg.keep-in.as.huge_quantity.	
		printk("[Sum-wake-q]: "); printk("WARN, Extra-wake-queue . too .. (while %c)\n", param3TSt);
		driver_dtxt_disp(db); 
		netif_wake_queue(dev);
	}
    */
	#endif	
}

static void toend_stop_queue1(struct net_device *dev, u16 stop_cnt)
{
	board_info_t *db= netdev_priv(dev);
	if (stop_cnt<NUM_QUEUE_TAIL)
		return; // true;
	if (stop_cnt==NUM_QUEUE_TAIL)
	{
	  	netif_stop_queue(dev);
		return; // true;
	}
	//.wrong path, but anyhow call stop for it
	netif_stop_queue(dev);
	printk("[.wrong path]: WARN, anyhow call stop for it .. ");
	printk("(cntStop %d)\n", db->bt.prob_cntStopped);
	driver_dtxt_disp(db); // OPTIONAL CALLED
	return; // false;
}
#endif

/* schwrk , WORK */

static void schwrk_loop_xmit(struct work_struct *work)
{
}

/*
 *  Set DM9051 multicast address
 */
static void
dm9000_hash_table_unlocked(struct net_device *dev)
{
	board_info_t *db = netdev_priv(dev);
	struct netdev_hw_addr *ha;
	int i, oft;
	u32 hash_val;
	u16 hash_table[4];
	u8 rcr = RCR_DIS_LONG | RCR_DIS_CRC | RCR_RXEN;

	for (i = 0, oft = DM9051_PAR; i < 6; i++, oft++)
		iiow(db, oft, dev->dev_addr[i]);

	/* Clear Hash Table */
	for (i = 0; i < 4; i++)
		hash_table[i] = 0x0;

	/* broadcast address */
	hash_table[3] = 0x8000;

	if (dev->flags & IFF_PROMISC)
		rcr |= RCR_PRMSC;

	if (dev->flags & IFF_ALLMULTI)
		rcr |= RCR_ALL;

	/* the multicast address in Hash Table : 64 bits */
	netdev_for_each_mc_addr(ha, dev) {
		hash_val = ether_crc_le(6, ha->addr) & 0x3f;
		hash_table[hash_val / 16] |= (u16) 1 << (hash_val % 16);
	}

	/* Write the hash table */
	for (i = 0, oft = DM9000_MAR; i < 4; i++) {
		iiow(db, oft++, hash_table[i]);
		iiow(db, oft++, hash_table[i] >> 8);
	}

	iow(db, DM9051_RCR, rcr);
	db->rcr_all= rcr;
/*
//TEST
	db->rcr_all |= RCR_PRMSC | IFF_ALLMULTI;
	printk("Test db->rcr_all from %02x to %02x\n", rcr, db->rcr_all);
*/	
}

//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------

static void dm9000_hash_table(struct work_struct *work)
{
	board_info_t *db = container_of(work, board_info_t, rxctrl_work);
	struct net_device *dev = db->ndev;

	printk("dm9 [ndo_set_multicast_list].s\n");

	//spin_lock_irqsave(&db->statelock, flags);
	mutex_lock(&db->addr_lock);
	dm9000_hash_table_unlocked(dev);
	
//=.e
	//spin_unlock_irqrestore(&db->statelock, flags);
    mutex_unlock(&db->addr_lock);
}

#if DM9051_CONF_TX
/*
 *  Send a packet from upper layer
 *  (JJ-20140225, When '!empty'
 *   in schwrk_loop_xmit_inRx() )
 */
static void dm9051_chk_tx(struct net_device *dev, u8 *wrptr)
{
#if 0
    printk("dm9.tx_packets %lu ", dev->stats.tx_packets);
    printk("tx(%02x %02x %02x %02x %02x %02x ", wrptr[0], wrptr[1],wrptr[2],wrptr[3],wrptr[4],wrptr[5]);
    printk("%02x %02x   %02x %02x %02x %02x ", wrptr[6], wrptr[7],wrptr[8],wrptr[9],wrptr[10],wrptr[11]);
    printk("%02x %02x\n", wrptr[12],wrptr[13]);
#endif
}

/*
 *  return: BOOL last 
 *  - true LAST PACKET 
 *  - false HAS NEXT PACKET 
 */
/*
static void dm9051_send_packet(board_info_t *db)
{
    struct net_device *dev = db->ndev;
    struct sk_buff *tx_skb;
    tx_skb = skb_dequeue(&db->txq);
  //last = skb_queue_empty(&db->txq);
	if (tx_skb != NULL) { // JJ-20140225, The skb has contain a packet
        if(db->bt.local_cntTXREQ==2)
        {
           while( ior(db, DM9051_TCR) & TCR_TXREQ ) 
             driver_dtxt_step(db, 'Z');
           db->bt.local_cntTXREQ= 0;
        }
		dm9051_outblk(db, tx_skb->data, tx_skb->len); // 'dm9051_wrpkt1'
		// Set TX length to DM9051
		iow(db, DM9051_TXPLL, tx_skb->len);
		iow(db, DM9051_TXPLH, tx_skb->len >> 8);
		iow(db, DM9051_TCR, TCR_TXREQ);	// Cleared after TX complete
        dev->stats.tx_bytes += tx_skb->len;
        dev->stats.tx_packets++;
		// done tx
		dm.9051_chk_tx(dev, tx_skb->data);
        dev_kfree_skb(tx_skb);	

		driver_dtxt_step(db, '1');
        db->bt.local_cntTXREQ++;
        db->bt.local_cntLOOP++;
	}
  //return last;
    return;
}

static void schwrk_loop_xmit_inRx(board_info_t *db)
 //From:
 //static void schwrk_loop_xmit_inRx(struct work_struct *work)
 //	board_info_t *db = container_of(work, board_info_t, tx_work);
{
	struct net_device *dev = db->ndev;
#if LOOP_XMIT
    mutex_lock(&db->addr_lock);

	/ *.
     *. while( ior(db, DM9051_TCR) & TCR_TXREQ ) 
     *.   driver_dtxt_step(db, 'B');
     * /
    db->bt.local_cntTXREQ= 0;
    db->bt.local_cntLOOP= 0;
	while(!skb_queue_empty(&db->txq)) // JJ-20140225, When '!empty'
	{
	#if 1
      while( ior(db, DM9051_TCR) & TCR_TXREQ ) 
        driver_dtxt_step(db, 'B');
	#endif		
	  dm9051_send_packet(db);
	}

	driver_dtxt_step(db, 'w');
	driver_dloop_step(db, db->bt.local_cntLOOP);

    opening_wake_queue1(dev); 
    mutex_unlock(&db->addr_lock);
#endif
}
*/
#endif

#if 1
void dm9051_fifo_show_flatrx(char *dstr, board_info_t *db)
{
	u16 rwregs[2];	     
	u16 calc;
	read_rwr(db, &rwregs[0]);
	read_mrr(db, &rwregs[1]);
	calc= dm9051_calc(rwregs[0], rwregs[1]);
	
	/* Show rx-occupied state */
	if (dstr) printk("%s: ", dstr);
	printk("rxWrRd .%04x/%04x (RO %d.%d%c", rwregs[0], rwregs[1], 
		calc>>8, calc&0xff, '%');
	printk(")\n");
}
#endif

#if 1
//(3)
/*
 *  Received a packet and pass to upper layer
 */	
	
struct dm9051_rxhdr0 { //old
	u8	RxPktReady;
	u8	RxStatus;
	__le16	RxLen;
} __packed;

struct spi_rxhdr { //new
	u8	padb;
	u8	spiwb;
	struct dm9051_rxhdr {
	  u8	RxPktReady;
	  u8	RxStatus;
	  __le16	RxLen;
	} rxhdr;
} __packed;

/*
 *  Received a packet and pass to upper layer
 */	
static void dm9000_rx(struct net_device *dev)
{
	board_info_t *db = netdev_priv(dev);
#if 1	
	struct dm9051_rxhdr rxhdr;
#endif	
#if 0	
	struct spi_rxhdr spihdr; //new
#endif	
	struct sk_buff *skb;
	u8 rxbyte, *rdptr;
	bool GoodPacket;
	int RxLen;
	u16 calc;
	
	db->rx_count= 0;

	/* Get most updated data */
	rxbyte= ior(db, DM_SPI_MRCMDX);	/* Dummy read */
	rxbyte= ior(db, DM_SPI_MRCMDX);	/* Dummy read */
		
	do {
		db->bC.RxLen = 0; // store.s
		// if ( rxbyte & DM9000_PKT_ERR) 
		// if (!(rxbyte & DM9000_PKT_RDY))
		// if ( rxbyte != DM9000_PKT_RDY)
		if ( rxbyte != DM9051_PKT_RDY)
		{
			if ( rxbyte == 0x00 ) {
				
			    db->bC.rxbyte_counter0++;
	
			    calc= dm9051_rx_cap(db); // get db->rwregs[0] & db->rwregs[1]
			    if (db->bC.rxbyte_counter0>1 || (calc>>8)>NUM_RX_FIFO_FULL){ // '50'
			         printk("ISRByte 0x%02x, rxWrRd .%04x/%04x (rxbyt==00 (%d ++) @ conti_cntr %d) RO %d.%d%c\n", 
			           db->bC.isbyte, db->rwregs[0], db->rwregs[1],
			           db->bC.rxbyte_counter0_to_prt, db->bC.rxbyte_counter0,
			     	   calc>>8, calc&0xFF, '%');			     	   
	     	   
				     db->bC.rxbyte_counter0_to_prt += 1;
			         printk("RXB_00Hs_or_FIFO_pre_full ");
			         driver_dtxt_disp(db);
			    }
			    else
				     db->bC.rxbyte_counter0_to_prt += 1;
			} else
			    db->bC.rxbyte_counter++; // insteaded (FFH, or not_01H)
			
		    
			dm9051_fifo_ERRO(rxbyte, db); // (00H/FFH/not_01H) // {Operate RST if continue 'n' 0x00 read.}
			return;
		} /* Status check: this byte must be 0xff, 0 or 1 */
		
		/* rxbyte_counter/rxbyte_counter0 */
		bcrdy_rx_info_clear(&db->bC);
 
		/* A packet ready now  & Get status/length */
		GoodPacket = true;
		
		
//	struct net_device *dev = db->ndev;
//	u16 calc= dm9051_rx_cap(db);
//    printk("hdrWrRd.s.%02x/%02x(RO %d.%d%c)\n", db->rwregs[0], db->rwregs[1], calc>>8, calc&0xFF, '%');
//    printk("hdrWrRd.e.%02x/%02x(RO %d.%d%c) rxlen= ...\n", db->rwregs[0], db->rwregs[1], calc>>8, calc&0xFF, '%');
        dm9051_disp_hdr_s(db);
        
		  dm9051_rd_rxhdr(db, (u8 *)&rxhdr, sizeof(rxhdr));
		
		  RxLen = le16_to_cpu(rxhdr.RxLen);
		  db->bC.RxLen = le16_to_cpu(rxhdr.RxLen); // store

        dm9051_disp_hdr_e(db, RxLen);
        
		/*
		 * [LARGE THAN 1536 or less than 64]!"
		 */
		 if (RxLen > DM9051_PKT_MAX || RxLen < 0x40) {

			u16 calc= dm9051_rx_cap(db);
			db->bC.LARGErr_counter++;
			
	     	printk("\n");
	     	printk("( LargEr %d / %d ) LargeLen=%d (RO %d.%d%c", 
	        	db->bC.LARGErr_counter, (db->bC.FIFO_RST_counter+1), 
	     		RxLen, calc>>8, calc&0xFF, '%');
	     	printk(")\n");
	     		
	        dm9051_fifo_reset(1, "dmfifo_reset( LargEr )", db);
	        dm9051_fifo_reset_statistic(db);
			printk("\n");
		    return;
		 }

		 /* Packet Status check, 'RSR_PLE' happen, but take it not error!! 20150609JJ */		 
		 /* rxhdr.RxStatus is identical to RSR register. */
		if (rxhdr.RxStatus & (RSR_FOE | RSR_CE | RSR_AE |
				      RSR_RWTO |
				      RSR_LCS | RSR_RF)) {
			GoodPacket = false;
			
	 		rdptr= (u8 *)&rxhdr;
     		printk("\n");
	 		printk("<!GoodPacket-rxbyte&rxhdr %02x & %02x %02x %02x %02x>\n", rxbyte, rdptr[0], rdptr[1], rdptr[2], rdptr[3]);
	 		
			if (rxhdr.RxStatus & RSR_FOE) 
				dev->stats.rx_fifo_errors++;
			if (rxhdr.RxStatus & RSR_CE) 
				dev->stats.rx_crc_errors++;
			if (rxhdr.RxStatus & RSR_RF) 
				dev->stats.rx_length_errors++;
				
			db->bC.StatErr_counter++;
	     	printk("\n");
	     	printk("( StatEr %d / %d ) StatEr=0x%02x", db->bC.StatErr_counter, (db->bC.FIFO_RST_counter+1), 
	     		rdptr[1]);
	     	printk("\n");
            dm9051_fifo_reset(1, "[!GoodPacket - StatusErr]", db);
            dm9051_fifo_reset_statistic(db);
            printk("\n");
		    return;
		}

		/* Move data from DM9051 */
		if ((skb = dev_alloc_skb(RxLen + 4)) == NULL)  {
            printk("dm9051 [Warn] [!ALLOC %d rx.len space]\n", RxLen);
            printk("\n");
            /* dump-data */
            dm9051_dumpblk(db, RxLen);
		    return;
		}

	    /* 
	     *  We note that "#define NET_IP_ALIGN  2"
	     *
	     *	Move data from DM9051 
		 *  (Linux skb->len IS LESS than 4, because it = RxLen - 4)
		 */
		/* Increase the headroom of an empty &skb_buff by            *
		 * reducing the tail room. Only allowed for an empty buffer. */ 
		skb_reserve(skb, 2);
		/* A pointer to the first byte is returned */
		rdptr = (u8 *) skb_put(skb, RxLen - 4);  
		
		/* Read received packet from RX SRAM */
	  //dm9051_rd_rxdata(db, rdptr-1, RxLen);
	  //=
		dm9051_inblk(db, rdptr-1, RxLen);

		if (!dm9051_chk_data(db, rdptr, RxLen))
			return;
    
		dev->stats.rx_bytes += RxLen;
    
		/* Pass to upper layer */
		skb->protocol = eth_type_trans(skb, dev);
		if (dev->features & NETIF_F_RXCSUM) {
			if ((((rxbyte & 0x1c) << 3) & rxbyte) == 0)
				skb->ip_summed = CHECKSUM_UNNECESSARY;
			else
				skb_checksum_none_assert(skb);
		}
		netif_rx(skb);
		dev->stats.rx_packets++;
		db->rx_count++;
		
		/* Get most updated data */
		rxbyte= ior(db, DM_SPI_MRCMDX);	/* Dummy read */
		rxbyte= ior(db, DM_SPI_MRCMDX);	/* Dummy read */ //rxbyte = 0x01; //. readb(db->io_data);

	} while (rxbyte == DM9051_PKT_RDY); // CONSTRAIN-TO: (rxbyte != XX)
}
//static irqreturn_t dm.9051_isr_ext(int irq, void *dev_id, int flag)=
//static irqreturn_t dm.9051_isr_func(int irq, void *dev_id, int flag)
static irqreturn_t dm9051_isr_ext(int irq, void *dev_id, int flag)
{
	struct net_device *dev = dev_id;
	board_info_t *db = netdev_priv(dev);
	u16 calc;
	u16 rwregs[2];
	int int_status;
	u8 rxbyte;
	
  #if DRV_INTERRUPT_1	
	  mutex_lock(&db->addr_lock);
	  iiow(db, DM9051_IMR, IMR_PAR); // Disable all interrupts 
  #elif DRV_POLL_0
	  mutex_lock(&db->addr_lock);
  #endif

	// dm9051_isr(irq, dev_id); =
	// Get DM9051 interrupt status 
	db->bC.isbyte= ior(db, DM9051_ISR);	
	int_status = db->bC.isbyte; // Got ISR
	iiow(db, DM9051_ISR, int_status);	// Clear ISR status 

	// Received the coming packet 
	calc= dm9051_rx_cap(db);
	if (db->rwregs[0]==db->rwregs[1])
	{
	  //;
	  if (int_status & ISR_ROS) 
	  	printk("( Impossible rwregs[0]==rwregs[1] overflow ) %d ++ \n", db->bC.rxbyte_counter0_to_prt);
	}
	else
	{
		rwregs[0]= db->rwregs[0];
		rwregs[1]= db->rwregs[1];
		//if (int_status & ISR_PRS)
		//	dm9000_rx(dev);
		//=
#if 1
		rxbyte= ior(db, DM_SPI_MRCMDX);	/* Dummy read */
		rxbyte= ior(db, DM_SPI_MRCMDX);	/* Dummy read */
		if ( rxbyte == DM9051_PKT_RDY)
		{
			if (int_status & ISR_PRS)
			{
				dm9000_rx(dev);
				//3p6s
				//if (db->rx_count>=5) /*typical, if (db->rx_count==1)*/
				//  printk("%2d.ISRByte NORM%02x rxb%02x (add %d to %lu) dtx %d\n", 
				//    flag, int_status, rxbyte, db->rx_count, dev->stats.rx_packets,
				//    db->bt.prob_cntStopped);
				if (db->rx_count==0)
				  printk("%2d.ISRByte 0x%02x NORMAL-but-Zero-pkt rxb%02x ( ??? %d to %lu)\n", 
				    flag, int_status, rxbyte, db->rx_count, dev->stats.rx_packets);
			}
			else
			{
				dm9000_rx(dev);
				//3p6s
				//if (db->rx_count>=5) /*typical, if (db->rx_count==1)*/
				  //printk("%2d.ISRByte what%02x rxb01 (add %d to %lu) dtx %d\n", 
				  //  flag, int_status, db->rx_count, dev->stats.rx_packets,
				  //  db->bt.prob_cntStopped);
			}
			
			if (db->bt.prob_cntStopped && ((calc>>8) >= 12) ) 
			  printk("%2d.rxb01 --rxDone tx found (-- .%04x/%04x RO %2d.%d%c)\n", flag, 
			    rwregs[0], rwregs[1], calc>>8, calc&0xFF, '%');
		}
		//.else
		//.{
		//.	if (int_status & ISR_PRS)
		//.	{
		//.		dm9000_rx(dev);
		//.		printk("%d.ISRByte 0x%02x warnning rxbyt=0x%02x (add %d to %lu)\n", 
		//.		  flag, int_status, rxbyte, db->rx_count, dev->stats.rx_packets);
		//.	}
		//.}
		else
		{
			if (db->bt.prob_cntStopped) 
			  printk("%2d.rxb%02x %d.%d%c ( - .%04x/%04x no-rx tx fnd -)\n", 
			    flag, rxbyte, calc>>8, calc&0xFF, '%', rwregs[0], rwregs[1]);
		}
#endif	
	}

	/* Receive Overflow */
	if (int_status & ISR_ROS){
#if 1
//early fifo_reset
		db->bC.RXBErr_counter++;
		printk("( Rxb %d ) %d ++ \n", db->bC.RXBErr_counter, db->bC.rxbyte_counter0_to_prt);
		if (!(db->bC.RXBErr_counter%5))
	     {
	       driver_dtxt_disp(db);
	       driver_dloop_disp(db);
	     }
#endif		
		printk(" db_isbyte 0x%02x (%d ++)\n", db->bC.isbyte, db->bC.rxbyte_counter0_to_prt);
		printk(" int_status 0x%02x", int_status);
		dm9051_fifo_show_flatrx(" [ERR-ISR] (recieve overflow)", db);
#if 1
//early fifo_reset
		dm9051_fifo_reset(1, "dmfifo_reset( RxbEr )", db);
	    dm9051_fifo_reset_statistic(db);
#endif
		printk("\n");
	}
	
  #if DRV_INTERRUPT_1
  #elif DRV_POLL_0
    mutex_unlock(&db->addr_lock);
  #endif

	//	if (int_status & ISR_LNKCHNG)
	//	  schedule_delayed_work(&db->phy_poll, 1); // fire a link-change request 

  #if DRV_INTERRUPT_1
	iiow(db, DM9051_IMR, db->imr_all); // Re-enable interrupt mask 
    mutex_unlock(&db->addr_lock);
  #elif DRV_POLL_0
  #endif	

	return IRQ_HANDLED; //[Here! 'void' is OK]
}
int rx_continue_rx(struct net_device *dev, board_info_t *db, int flag)
{
	u16 calc;
	
	if (db->bt.prob_cntStopped) { 
      if (db->DISPLAY_rwregs[0]!=db->rwregs[0] || db->DISPLAY_rwregs[1]!=db->rwregs[1]) {
        db->DISPLAY_rwregs[0]= db->rwregs[0];
        db->DISPLAY_rwregs[1]= db->rwregs[1];
        calc= dm9051_rx_cap_lock(db); 
        if (db->rwregs[0]!=db->rwregs[1]) 
          printk("%2d.break %d.%d%c .%04x/%04x\n", flag-1, 
	    calc>>8, calc&0xFF, '%', db->rwregs[0], db->rwregs[1]); 
      }
	}
    if (db->bt.prob_cntStopped) return 0; //false //break;
	dm9051_isr_ext(dev->irq, dev, flag);
	return 1; //true;
}
#endif

//(3.1)
static void dm9051_continue_poll(struct work_struct *work) //old. dm9051_INTP_isr
{
	u8 nsr;
	int link;
	
	struct delayed_work *dw = to_delayed_work(work);
	board_info_t *db = container_of(dw, board_info_t, rx_work);
  //board_info_t *db = container_of(work, board_info_t, rx_work);

	struct net_device *dev = db->ndev;
	
	mutex_lock(&db->addr_lock);
	nsr= iior(db, DM9051_NSR); 
	mutex_unlock(&db->addr_lock);
	
	//JJ-Add
	link= !!(nsr & 0x40); //& NSR_LINKST
	db->link= link;       //Rasp-save
	
//	printk("[DM9051.poll] nsr = [%d], [%x] 22222222222222222\n", nsr, nsr);
//	if (nsr==64) { 
//		printk("[DM9051.poll] nsr = [%d], link= %d to %d\n", nsr, link, 1);
//		link= 1;
//	}
	
	if (netif_carrier_ok(dev) != link) {
		if (link)
		  netif_carrier_on(dev);
		else
		  netif_carrier_off(dev);
		printk("[DM9051.poll] Link Status is: %d\n", link);
	}
	
#if DM9051_CONF_TX
#if 1	
	//printk("dm9<DBG xmit_inRX.s>\n");
	if (db->bt.prob_cntStopped)  // This is more exactly right!!
    {
	 #if 0 
	  //=
	  schwrk_loop_xmit_inRx(db); // This is more exactly right!!
	  //=
	 #else // 0
  #if LOOP_XMIT
    int nTx;
    u16 txcalc;
    mutex_lock(&db->addr_lock);
 //.txcalc= dm9051_tx_cap(db);
 //.dm9051_display_tx_cap("txSta.", db, 0, nTx, txcalc);
    nTx= 0;
    db->bt.local_cntTXREQ= 0;
    db->bt.local_cntLOOP= 0;
	while(!skb_queue_empty(&db->txq)) // JJ-20140225, When '!empty'
	{
		  struct sk_buff *tx_skb;
        
		  //  =dm9051_send_packet(db);	
		  while( ior(db, DM9051_TCR) & TCR_TXREQ ) 
		    driver_dtxt_step(db, 'B');
        
		  if (!nTx) {
		    txcalc= dm9051_tx_cap(db);
		    dm9051_display_tx_cap("txGnd.", db, 1, nTx, txcalc);
		    dm9051_display_get_txrwr_sectloop();
		  }
    
		  //if (1)
		  //{
			  tx_skb = skb_dequeue(&db->txq);
			  if (tx_skb != NULL) {
			  	
			        if(db->bt.local_cntTXREQ==2)
			        {
			           while( ior(db, DM9051_TCR) & TCR_TXREQ ) 
			             driver_dtxt_step(db, 'Z');
			           db->bt.local_cntTXREQ= 0;
			        }
				    /* Set TX length to DM9051 */
				    nTx++;
				    dm9051_outblk(db, tx_skb->data, tx_skb->len);
				    
				    dm9051_display_get_txrwr_triploop(db, nTx, tx_skb->len); // do-prt "   9Tx_ON_small(%d ... "
					
				    iow(db, DM9051_TXPLL, tx_skb->len);
				    iow(db, DM9051_TXPLH, tx_skb->len >> 8);
				    iow(db, DM9051_TCR, TCR_TXREQ);
				    dev->stats.tx_bytes += tx_skb->len;
				    dev->stats.tx_packets++;
				    /* done tx */
			        #if 1
				    dm9051_chk_tx(dev, tx_skb->data);
			        #endif
				    dev_kfree_skb(tx_skb);	
				    
				    driver_dtxt_step(db, '1');
		            db->bt.local_cntTXREQ++;
		            db->bt.local_cntLOOP++;
			  }
		  //}
		}
		dm9051_display_get_txrwr_endloop(nTx);

	driver_dtxt_step(db, 'w');
	driver_dloop_step(db, db->bt.local_cntLOOP);
    
    opening_wake_queue1(dev); 
    txcalc= dm9051_tx_cap(db);
    dm9051_display_tx_cap("txDone", db, 2, nTx, txcalc);
    mutex_unlock(&db->addr_lock);
  #endif //LOOP_XMIT
	 #endif // 0
    } // db->bt.prob_cntStopped
#else	
	schwrk_loop_xmit_inRx(db);
#endif	
#endif //DM9051_CONF_TX
	
	do {
	
   //.printk("dm9<DBG (to dm.9051_isr_EXT).s>\n");
	  if (!rx_continue_rx(dev, db, 1)) break;
	  if (!rx_continue_rx(dev, db, 2)) break;
	  if (!rx_continue_rx(dev, db, 3)) break;
	  if (!rx_continue_rx(dev, db, 4)) break;
	  if (!rx_continue_rx(dev, db, 5)) break;
	  if (!rx_continue_rx(dev, db, 6)) break;
	  if (!rx_continue_rx(dev, db, 7)) break;
	  if (!rx_continue_rx(dev, db, 8)) break;
	  if (!rx_continue_rx(dev, db, 9)) break;
	  if (!rx_continue_rx(dev, db, 10)) break;
	  if (!rx_continue_rx(dev, db, 11)) break;
	  if (!rx_continue_rx(dev, db, 12)) break;
    if (db->bt.prob_cntStopped) {       
      u16 calc;
      if (db->DISPLAY_rwregs[0]!=db->rwregs[0] || db->DISPLAY_rwregs[1]!=db->rwregs[1]) {
        db->DISPLAY_rwregs[0]= db->rwregs[0];
        db->DISPLAY_rwregs[1]= db->rwregs[1];
        calc= dm9051_rx_cap_lock(db); 
        if (db->rwregs[0]!=db->rwregs[1]) 
          printk("%2d.break %d.%d%c .%04x/%04x\n",  
	    12, calc>>8, calc&0xFF, '%', db->rwregs[0], db->rwregs[1]); 
      }
	}

	} while (0);

  #if DRV_INTERRUPT_1	
	enable_irq(db->ndev->irq);
  #elif DRV_POLL_0
	dm9051_INTPschedule_isr(db); 
    /*old is schedule_work(&db->rx_work); */
  #endif
}

//************************************************************************************


#if DRV_INTERRUPT_1
//(3.1)
static irqreturn_t dm9051_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	board_info_t *db = netdev_priv(dev);
	disable_irq_nosync(irq);
    dm9051_INTPschedule_isr(db); //old schedule_work(&db->rx_work);
	return IRQ_HANDLED;
}
#endif

#if DRV_INTERRUPT_1 | DRV_POLL_0
static void dm9051_INTPschedule_isr(board_info_t *db)
{
    /* 0, Because @delay: number of jiffies to wait or 0 for immediate execution */
    schedule_delayed_work(&db->rx_work, 0); 
}

//=
//static irqreturn_t dm9051_isr(int irq, void *dev_id) // discare
//{
//	return IRQ_WAKE_THREAD; //'IRQ_HANDLED'
//}
#endif

//--------------------------------------------------------------------------------------
/* routines for dbg tasks handling */
//--------------------------------------------------------------------------------------
/* routines for logic tasks handling */
//--------------------------------------------------------------------------------------
/* MII interface controls */
//--------------------------------------------------------------------------------------

//(1)
//(2)
//(3)
//(4)
void dumphex_fifo(char *typ, unsigned len, u8 *pbff, unsigned start_loc) //dm9051_dumphex
{
	unsigned i;
	u16 adr= start_loc; //0;
	i=0;
	printk("dumphex_fifo:('%s') PktLen %d : loc %d", typ, start_loc, len); //": %x" , (unsigned int) pbff
	while (len--){
		if (!(i%8)) printk(" ");
		if (!(i%16)) printk("\n%04x ", adr);
		i++;
		printk(" %02x", *pbff++);
		adr++;
	}
	printk("\n");
}

//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------

#if DRV_INTERRUPT_1
static irqreturn_t dm951_irq(int irq, void *pw)
{
	board_info_t *db = pw;
//r	disable_irq_nosync(irq);
    schedule_work(&db->rx_work); //new 'dm9051_INTPschedule_isr'
	return IRQ_HANDLED;
}
#endif

/* device_ops and probe  */

static void dm9051_open_code(struct net_device *dev, board_info_t *db) // v.s. dm9051_probe_code()
{
	mutex_lock(&db->addr_lock);
	
    /* Note: Reg 1F is not set by reset */
    iow(db, DM9000_GPR, 0);	/* REG_1F bit0 activate phyxcer */
    mdelay(1); /* delay needs by DM9051 */ 
	
    /* Initialize DM9051 board */
    dm9051_reset(db);
	dm9051_init_dm9051(dev);
	
	mutex_unlock(&db->addr_lock);
}

//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------

#if 1
/**
 * 9051_open - open network device
 * @dev: The network device being opened.
 *
 * Called when the network device is marked active, such as a user executing
 * 'ifconfig up' on the device.
 */
static int dm9051_open(struct net_device *dev)
{
	board_info_t *db = netdev_priv(dev);
	unsigned char *int_pol;
	unsigned  rdv;
    printk("\n");
	printk("[dm951_open].s\n");

    dm9051_power_en(1);

  #if 1
	if (netif_msg_ifup(db))
		dev_dbg(&db->spidev->dev, "enabling %s\n", dev->name);
  #endif

	if (db->chip_code_state==CCS_NUL)
	  dm9051_open_code(dev, db);
	
	mutex_lock(&db->addr_lock); //Note: must 

	   rdv= iior(db, DM9051_INTCR);
	   int_pol= "active high";
	   if (rdv&0x01)
	     int_pol= "active low";
	   printk("ior[REG39H][%02x] (b0: %d)(%s)\n", rdv, rdv&0x01, int_pol);

     #if 0
     //[    6.644346] Unable to handle kernel NULL pointer dereference at virtual address 00000000
     //[    6.652369] pgd = dbe30000
     //     ...
     //[    6.675053] PC is at 0x0
     //[    6.677572] LR is at mii_link_ok+0x1c/0x38
     //[    6.681640] pc : [<00000000>]    lr : [<c04058f0>]    psr: 60000013

     //
     //  [In spi driver] No acceptable to call 'mii_check_media()'
     //
     /* mii_check_media(&db->mii, netif_msg_link(db), 1); */
     #endif

     #if DM9051_CONF_TX
     //[Which one is mandotory?]
     //If is 'netif_carrier_on(dev);' 
     // WILL solve others by move to a poll_phy (work)
     printk("[  sum  ] skb_queue_head_init: ?\n");
     printk("[  add  ] No, Only is in _probe())\n");
     printk("[  add  ] HERE, Test call skb_queue_head_init())\n");
#if 1
    //[Init.] [Re-init compare to ptobe.]
	 skb_queue_head_init(&db->txq); 
	 db->tx_eq= 0;
	 db->tx_err= 0;
#endif
     //[Should operated in _probe()]
     // Or that you can only call 'netif_stop_queue(dev)' before this operate (i.e. in _probe()),
     // And that you can not call 'netif_wake_queue(dev)' before this operate (i.e. in _probe()).
     printk("[  sum  ] netif_start_queue: ?\n");
     printk("[  add  ] Yes, call netif_start_queue()\n");
	 netif_start_queue(dev);
     #endif	

	 /* Init driver variable */
	
  #if 1
  #if DRV_POLL_0
	/* Flexable, So can start in Probe or here! */
	if (db->driver_state==DS_POLL)
	  ;
	else {
	  db->driver_state= DS_POLL;
	  //org=
	   schedule_delayed_work(&db->rx_work, 0); 
       //dm9051_INTPschedule_isr(db); 
       //schedule_work(&db->rx_work);

       //printk("[  sum  ] netif_carrier_on: ?\n");
       //printk("[  add  ] Yes, call netif_carrier_on()\n");
       //netif_carrier_on(dev);
	  
	  //add=
	  // netif_carrier_on(dev); have handled inside 'db->rx_work's hook-function.
	}
  #endif
  #endif
  //dm_schedule_phy(db);

	mutex_unlock(&db->addr_lock);

  #if DM9051_CONF_TX
    opening_wake_queue1(dev);
  #endif	
    printk("[dm9.open].e (%s)\n", DRV_VERSION);
    printk("[dm9.open].d DM9051_SPI: Mode %s\n", MSTR_MOD_VERSION);
    printk("[dm9.open].d DM9051_RD: Burst size %s\n", RD_MODEL_VERSION);
    printk("[dm9.open].d DM9051_TX: Burst size %s\n", WR_MODEL_VERSION);
    printk("\n");
	return 0;
} 
// "DRV.DM9: SPI_MODE: SPI_Master DMA model.."
// "DRV.DM9: SPI_MODE: SPI_Master FIFO model.."
// "DM9051_RD: Burst size (no-limitation)"
// "DM9051_TX: Burst size (no-limitation)"

/**
 * dm951_net_stop - close network device
 * @dev: The device being closed.
 *
 * Called to close down a network device which has been active. Cancell any
 * work, shutdown the RX and TX process and then place the chip into a low
 * power state whilst it is not being used.
 */
static int dm9000_stop(struct net_device *dev)
{
    board_info_t *db = netdev_priv(dev);
    /* "kernel_call.ndo_set_multicast_list.first". */
    /* Then.call.ndo_stop                          */
	
    printk("[dm951_stop].s\n");
    db->driver_state= DS_IDLE;
    db->chip_code_state= CCS_NUL;
	
  #if DRV_INTERRUPT_1 | DRV_POLL_0
    flush_delayed_work(&db->rx_work);
	//cancel_delayed_work_sync(&db->rx_work); //flush_work(&db->rx_work);
  #endif
    cancel_delayed_work_sync(&db->phy_poll);

    toend_stop_queue1(dev, db->bt.prob_cntStopped= NUM_QUEUE_TAIL); //ending_stop_queue1(dev);
	  
    //JJ-Count-on
    netif_carrier_off(dev);

    /* dm9051_shutdown(dev) */
    mutex_lock(&db->addr_lock);
    dm9051_phy_write(dev, 0, MII_BMCR, BMCR_RESET);	/* PHY RESET */
    iow(db, DM9000_GPR, 0x01);	/* Power-Down PHY */
    iow(db, DM9051_IMR, IMR_PAR);	/* Disable all interrupt */
    iow(db, DM9051_RCR, RCR_RX_DISABLE);	/* Disable RX */
    mutex_unlock(&db->addr_lock);

    dm9051_power_en(0);

    return 0;
}

#if 1
static netdev_tx_t dm9051_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	board_info_t *db = netdev_priv(dev);

	//[migrated from _start_xmit].s	
	#if 1
	
	spin_lock(&db->statelock);//mutex_lock(&db->addr_lock);
#if DM9051_CONF_TX
    /*
    toend_stop_queue1(dev, db->bt.prob_cntStopped++); //if !optional_stop_queue1(dev)...
    =or ...
    */	
	toend_stop_queue1(dev, db->bt.prob_cntStopped++ );

	skb_queue_tail(&db->txq, skb); // JJ: a skb add to the tail of the list '&db->txq'
	driver_dtxt_step(db, '0'); //driver_dtxt_step(db, 'q'); // Normal
#endif
	spin_unlock(&db->statelock);//mutex_unlock(&db->addr_lock);
	
	#endif
	//[migrated from _start_xmit].e

	schedule_work(&db->tx_work);  //'sch'
	return NETDEV_TX_OK;
}
#endif
static void dm9051_set_multicast_list(struct net_device *dev)
{
	#if 0
	board_info_t *db = netdev_priv(dev);
	//spin_lock(&db->statelock); /* no need */
	//spin_unlock(&db->statelock); /* no need */
	#endif

	board_info_t *db = netdev_priv(dev);
	schedule_work(&db->rxctrl_work);
}

//static int dm9051_ioctl(struct net_device *dev, struct ifreq *req, int cmd)
//{
//	board_info_t *dm = to_dm9051_board(dev);

//	if (!netif_running(dev))
//		return -EINVAL;

//	return generic_mii_ioctl(&dm->mii, if_mii(req), cmd, NULL);
//}

static const struct net_device_ops dm9051_netdev_ops = {
	.ndo_open		= dm9051_open,
	.ndo_stop		= dm9000_stop,
	.ndo_start_xmit		= dm9051_start_xmit,
#if 1
	//> KERNEL_VERSION(3, 2, 0)
	.ndo_set_rx_mode = dm9051_set_multicast_list,
  //.ndo_set_multicast_list	= dm9051_set_multicast_list, /* dm9051_hash_table, 2014.04.02_DM951 */
#endif	
	.ndo_set_mac_address	= eth_mac_addr,
	
    //.ndo_do_ioctl		= dm9051_ioctl,

	.ndo_change_mtu		= eth_change_mtu,
	.ndo_validate_addr	= eth_validate_addr,
};
#endif

void dm9051_probe_code(struct net_device *dev, board_info_t *db) // v.s. dm9051_open_code()
{
	unsigned  probe_dat;
    /* Note: Reg 1F is not set by reset */
    iow(db, DM9000_GPR, 0);	/* REG_1F bit0 activate phyxcer */
    mdelay(1); /* delay needs by DM9051 */ 
	
    /* Initialize DM9051 board */
    dm9051_reset(db);
	dm9051_init_dm9051(dev);
	
	probe_dat= ior(db, DM9051_ISR);
	printk("[dm9051.probe.ISR.MBNDRY_STS] data[= 0x%02x]\n", probe_dat);
}

#define	INNO_GENERAL_ERROR	1

#if 1 
  /* DMA3_P1 */
/*3p*/
static int SUBLCD_SPI_Init(struct board_info *db, int enable)
{
	mutex_lock(&db->addr_lock);
	if(enable){

#if DMA3_P1
		struct mt_chip_conf* spi_par; // added 2015-12-3
		SPI_GPIO_Set(1);
	
        spi_par = (struct mt_chip_conf *) db->spidev->controller_data;
		if(!spi_par){
			printk("[dm95_spi] spi config fail");
			mutex_unlock(&db->addr_lock);
			return INNO_GENERAL_ERROR;
		}
		spi_par->setuptime = 15; 
		spi_par->holdtime = 15; 
		spi_par->high_time = 2;       //10--6m   15--4m   20--3m  30--2m  [ 60--1m 120--0.5m  300--0.2m]
		spi_par->low_time = 2;
		spi_par->cs_idletime = 20; 

		spi_par->rx_mlsb = 1; 
		spi_par->tx_mlsb = 1;		 
		spi_par->tx_endian = 0;
		spi_par->rx_endian = 0;

		spi_par->cpol = 0;
		spi_par->cpha = 0;
#if DMA3_P2_MSEL_MOD
		spi_par->com_mod = DMA_TRANSFER;
#else
		spi_par->com_mod = FIFO_TRANSFER;
#endif

		spi_par->pause = 0;
		spi_par->finish_intr = 1;
		spi_par->deassert = 0;
#if 1
		if(spi_setup(db->spidev)){
			printk("[dm95_spi] spi_setup fail\n");
			mutex_unlock(&db->addr_lock);
			return INNO_GENERAL_ERROR;
		}
#endif

#else
              /*
		if(spi_setup(db->spidev)){
			printk("[dm95_spi] spi_setup fail\n");
			mutex_unlock(&db->addr_lock);
			return INNO_GENERAL_ERROR;
		}
              */
#endif
	}
	mutex_unlock(&db->addr_lock);
	return 0;
}/* */
#endif

/*
 * Search DM9051 board, allocate space and register it
 */
static int /* __devinit */ 
dm9051_probe(struct spi_device *spi)
{
	struct board_info *db;
	struct net_device *ndev;
    unsigned  chipid;
	int i;
	const unsigned char *mac_src;
	int ret = 0;

    printk("[ *dm9051  ] {_probe.s}\n");

    dm9051_power_en(1);

	ndev = alloc_etherdev(sizeof(struct board_info));
	if (!ndev) {
		dev_err(&spi->dev, "failed to alloc ethernet device\n");
		return -ENOMEM;
	}
	db = netdev_priv(ndev);
	
	mutex_init(&db->addr_lock);
	mutex_init(&db->sublcd_mtkspi_mutex);
	
	db->ndev = ndev;
	db->spidev = spi;
	spi->bits_per_word = 8;
//#if DMA3_P2_MSEL_MOD
        
       SUBLCD_SPI_Init(db, 1); // spi, // dma

       db->driver_state= DS_NUL;
       db->chip_code_state= CCS_NUL;
	
       db->DISPLAY_rwregs[0]= 0;
       db->DISPLAY_rwregs[1]= 0;
       db->link= 0;
	
#if 1
#if DM9051_CONF_TX
	driver_dtxt_init(db);
	driver_dloop_init(db);
#endif
#endif
	
	/* ERRO_counter/RXBErr_counter/LARGErr_counter/StatErr_counter/FIFO_RST_counter */
	bcprobe_rst_info_clear(&db->bC);
	/* rx_brdcst_counter/rx_multi_counter/rx_unicst_counter.rxbyte_counter/rxbyte_counter0/rxbyte_counter0_to_prt */
	bcopen_rx_info_clear(&db->bC); 
	
#if 1
#if DM9051_CONF_TX
	toend_stop_queue1(ndev, db->bt.prob_cntStopped= NUM_QUEUE_TAIL); //ending_stop_queue1(ndev);
#endif
#endif
	
#if 1	
	spin_lock_init(&db->statelock); // used in 'dm9051' 'start' 'xmit'
#endif
	INIT_WORK(&db->tx_work, schwrk_loop_xmit);
	INIT_WORK(&db->rxctrl_work, dm9000_hash_table);
#if DRV_INTERRUPT_1 | DRV_POLL_0
    INIT_DELAYED_WORK(&db->rx_work, dm9051_continue_poll); // old. 'dm9051_INTP_isr()' by "INIT_WORK"
#endif
	INIT_DELAYED_WORK(&db->phy_poll, dm_poll_phy);

	/* initialise pre-made spi transfer messages */
	spi_message_init(&db->spi_msg1);
	spi_message_add_tail(&db->spi_xfer1, &db->spi_msg1);

	/* setup mii state */
	db->mii.dev	     = ndev;
	db->mii.phy_id_mask  = 1;   //db->mii.phy_id_mask  = 0x1f;
	db->mii.reg_num_mask = 0xf; //db->mii.reg_num_mask = 0x1f;
	db->mii.phy_id		= 1,
	db->mii.mdio_read    = dm9051_phy_read;
	db->mii.mdio_write   = dm9051_phy_write;

	skb_queue_head_init(&db->txq); //[Init.]
    
	SET_NETDEV_DEV(ndev, &spi->dev);

#if 1
    /*
	 * No need: db->dev = &pdev->dev;            
     * May need: dev_set_drvdata(&spi->dev, db); 
     */
    dev_set_drvdata(&spi->dev, db);
#endif

    /* issue a global soft reset to reset the device. */
	dm9051_reset(db);

	/* Get chip ID */
    printk("[ *dm9051  ] {_probe.m} DM9051 chipID= 0x%x | 0x%x\n", DM9051_ID, DM9000_ID);
    //printk("[ *dm9051  ] CONFIG SPI speed[= %d]\n", dm9051_spi_board_devs[0].max_speed_hz);
	for (i = 0; i < 8; i++) {
      chipid= ior(db, DM9051_PIDL);
	  chipid |= (unsigned)ior(db, DM9051_PIDH) << 8; //ior(db, );
      //printk("[dm9051.rdreg8().chipid, reg 0x%02x/0x%02x= 0x%04x\n", DM9051_PIDL, DM9051_PIDH, chipid);
	}
	if (chipid != (DM9051_ID>>16) && chipid != (DM9000_ID>>16)) {
	  dev_err(&spi->dev, "failed to read device ID\n");
	  ret = -ENODEV;
	  goto err_id;
	}
//.	if (chipid == (DM9000_ID>>16)) printk("[dm9051.ior_chipid(): 0x%04x\n", chipid);
//.	if (chipid == (DM9051_ID>>16)) printk("[dm9051.ior_chipid(): 0x%04x\n", chipid);
	
    printk("[dm9051.dump_eeprom():");
	for (i = 0; i < 64; i++) {
		dm9051_read_eeprom(db, i, db->TxDatBuf);
		if (!(i%8)) printk("\n ");
		if (!(i%4)) printk(" ");
		printk(" %02x %02x", db->TxDatBuf[0], db->TxDatBuf[1]);
	}
	printk("\n");
	
#if 1
	db->TxDatBuf[0]= 0x80;
	db->TxDatBuf[1]= 0x41; //  0x0180 | (1<<14), DM9051 E1 (old) set WORD7.D14=1 to 'HP Auto-MDIX enable'
	dm9051_write_eeprom(db, (14 + 0) / 2, db->TxDatBuf);
    printk("[dm9051.write_eeprom():  WORD[%d]= %02x %02x\n",
    	(14 + 0) / 2, db->TxDatBuf[0], db->TxDatBuf[1]);
#endif

    printk("[dm9051.dump_eeprom():");
	for (i = 0; i < 16; i++) {
		dm9051_read_eeprom(db, i, db->TxDatBuf);
		if (!(i%8)) printk("\n ");
		if (!(i%4)) printk(" ");
		printk(" %02x %02x", db->TxDatBuf[0], db->TxDatBuf[1]);
	}
	printk("\n");
	
	/* The node address from the attached EEPROM(already loaded into the chip), first. */
    mac_src = "eeprom2chip";
   
    printk("[dm9051.ior_mac().a:\n");
	for (i = 0; i < 6; i++) {
      ndev->dev_addr[i]= ior(db, DM9051_PAR+i);
      printk(" %02x\n", ndev->dev_addr[i]);
	}
	printk("\n");
    
	/* The node address by the laboratory fixed (if previous not be valid) */
    if (!is_valid_ether_addr(ndev->dev_addr)) {
	  //mac_src = "random";
	    mac_src = "lab_fixed";
	    dm9051_init_mac(db);
	
	    printk("[dm9051.ior_mac().b:\n");
		for (i = 0; i < 6; i++) {
	      chipid= ior(db, DM9051_PAR+i);
	      printk(" %02x", chipid);
		}
		printk("\n");
	}
    
/*r	ret = request_irq(spi->irq, dm951_irq, IRQF_TRIGGER_LOW,
			  ndev->name, db); */

	ndev->if_port = IF_PORT_100BASET;
	ndev->irq = spi->irq;	

	/*
     * _DBG_.	ether_setup(ndev); --driver system function (no this is OK..?)
     */
	ndev->netdev_ops	= &dm9051_netdev_ops;
    //SET_ETHTOOL_OPS(ndev, &dm9051_ethtool_ops);
    //SET_ETHTOOL_OPS(ndev, &dm9051_ethtool_ops);
    //ndev->ethtool_ops = &dm9051_ethtool_ops;
#if DMA3_P3
      SET_ETHTOOL_OPS(ndev, &dm9051_ethtool_ops); /*3p*/  
#else
      ndev->ethtool_ops = &dm9051_ethtool_ops;
#endif

	ret = register_netdev(ndev);
    if (ret) {
		dev_err(&spi->dev, "failed to register network device\n");
        printk("[  dm9051  ] dm9051_probe {failed to register network device}\n");
        goto err_netdev;
    }

    #if 1
    //[TEST NOT TO BE IN PROBE...]
    /* */
    //db->chip_code_state= CCS_NUL;
    db->chip_code_state= CCS_PROBE;
    dm9051_probe_code(ndev, db);
    
    #endif

#if 1
//[Add_for_a_test!]
    db->driver_state= DS_NUL;
	//org=
	db->driver_state= DS_POLL;
	schedule_delayed_work(&db->rx_work, 0); 
#endif	

    printk("%s: dm951 is bus_num %d, chip_select %d\n", 
           ndev->name,
    	   spi->master->bus_num, 
    	   spi->chip_select);
	printk("%s: dm9051spi at isNO_IRQ %d MAC: %pM\n", // (%s)
		   ndev->name,
		   ndev->irq,
		   ndev->dev_addr); //, mac_src
    printk("[*dm9.probe].e (%s)\n", DRV_VERSION);
    printk("[*dm9.probe].d DM9051_SPI: Mode %s\n", MSTR_MOD_VERSION);
    printk("[*dm9.probe].d DM9051_RD: Burst size %s\n", RD_MODEL_VERSION);
    printk("[*dm9.probe].d DM9051_TX: Burst size %s\n", WR_MODEL_VERSION);
	return 0;

err_netdev:
#if DRV_INTERRUPT_1
	free_irq(spi->irq, db); 
#endif
err_id:

#if DRV_INTERRUPT_1
err_irq:
#endif
	printk("[ *dm9051  ] {_probe.e}not found (%d)\n", ret);
	free_netdev(ndev);
	return ret;
} // ['mutex_init' only in 'probe']
  
//**************************************************************************************
#if DMA3_P4
/*3p*/
static int dm9000_drv_suspend(struct spi_device *spi, pm_message_t state)
{
    board_info_t *db = dev_get_drvdata(&spi->dev);
    struct net_device *ndev = db->ndev;
       if (ndev) {
		db->in_suspend = 1;
		if (!netif_running(ndev)) {
		    netif_device_detach(ndev);
            dm9000_stop(ndev);
	    }
	}
	return 0;
}

static int dm9000_drv_resume(struct spi_device *spi)
{
    board_info_t *db = dev_get_drvdata(&spi->dev);
    struct net_device *ndev = db->ndev;
	if (ndev) {
		if (netif_running(ndev)) {
            dm9051_open(ndev);
			netif_device_attach(ndev);
		}

		db->in_suspend = 0;
	}
	return 0;
}/* */  
#endif

//--------------------------------------------------------------------------------------

/* driver bus management functions */
static int /* __devexit */ dm9000_drv_remove(struct spi_device *spi)
{
    board_info_t *db = dev_get_drvdata(&spi->dev);
	unregister_netdev(db->ndev);
#if DRV_INTERRUPT_1
   	free_irq(spi->irq, db);
#endif
	free_netdev(db->ndev);
	return 0;
}

struct spi_device_id dm9051_spi_id_table = {"dm9051", 0}; //DRVNAME_9051

static struct spi_driver dm9051_driver = {
	.driver	= {
		.name  = DRVNAME_9051, //"dm9051"
		.owner = THIS_MODULE,
	},
	.probe   = dm9051_probe,
	.remove  = /*__devexit_p*/(dm9000_drv_remove),
	.id_table = &dm9051_spi_id_table,
#if DMA3_P4
/*3p*/	.suspend = dm9000_drv_suspend,
/*3p*/	.resume = dm9000_drv_resume,
#endif
};

/*
#define SMDK_MMCSPI_CS 0

static struct s3c64xx_spi_csinfo smdk_spi0_csi[] = {
	[SMDK_MMCSPI_CS] = {
		.line = S5PV210_GPB(1),
		.set_level = gpio_set_value,
		.fb_delay = 0x2,
	},
};
*/

#if DMA3_P6
 // Use "spi_register_board_info(dm9051_spi_board_devs, ARRAY_SIZE(dm9051_spi_board_devs));" directly.
#else

 //..vsdnlne........

static unsigned verbose = 3;
module_param(verbose, uint, 0);
MODULE_PARM_DESC(verbose,
"0 silent, >0 show gpios, >1 show devices, >2 show devices before (default=3)");

static struct spi_device *spi_device;

static void dm9051_device_spi_delete(struct spi_master *master, unsigned cs)
{
	struct device *dev;
	char str[32];

	snprintf(str, sizeof(str), "%s.%u", dev_name(&master->dev), cs);

	dev = bus_find_device_by_name(&spi_bus_type, NULL, str);
	if (dev) {
		if (verbose)
			pr_info(DRVNAME_9051": Deleting %s\n", str);
		device_del(dev);
	}
}
static int dm9051_spi_register_board_info(struct spi_board_info *spi, unsigned n)
{
      /* Joseph_20151030: 'n' is always const is 1, in this design */

	struct spi_master *master;

	master = spi_busnum_to_master(spi->bus_num);
	if (!master) {
		pr_err(DRVNAME_9051 ":  spi_busnum_to_master(%d) returned NULL\n",
								spi->bus_num);
		return -EINVAL;
	}
	/* make sure it's available */
	dm9051_device_spi_delete(master, spi->chip_select);
	spi_device = spi_new_device(master, spi);
	put_device(&master->dev);
	if (!spi_device) {
		pr_err(DRVNAME_9051 ":    spi_new_device() returned NULL\n");
		return -EPERM;
	}
	return 0;
}
#endif

static int /* __init */ dm9051_init(void)
{
	printk("\n");
	printk("%s Driver\n",
		CARDNAME_9051);
	printk("%s Driver, V%s (%s)\n", 
		CARDNAME_9051, 
		DRV_VERSION, str_drv_xmit_type);
	//printk("%s Driver, %s\n", 
		//CARDNAME_9051, MASTER_MODEL_VERSION);
    printk("%s, SPI %s\n", CARDNAME_9051, MSTR_MOD_VERSION);
    printk("%s, RD %s\n", CARDNAME_9051, RD_MODEL_VERSION);
    printk("%s, TX %s\n", CARDNAME_9051, WR_MODEL_VERSION);
  #if 1
	printk("dm9r Driver, modalias in dm9051.h= %s\n", DRVNAME_9051);
	printk("dm9r Driver, modalias in dm9r.c= %s\n", dm9051_driver.driver.name);
  #endif		
  
#if DMA3_P6
//  spi_register_board_info(dm9051_spi_board_devs, ARRAY_SIZE(dm9051_spi_board_devs));
    spi_register_board_info(dm9051_spi_board_devs, ARRAY_SIZE(dm9051_spi_board_devs));
#else
    dm9051_spi_register_board_info(dm9051_spi_board_devs, ARRAY_SIZE(dm9051_spi_board_devs));
#endif
	return spi_register_driver(&dm9051_driver);
}

module_init(dm9051_init);

static void /*__exit */ dm9051_cleanup(void)
{
//#ifdef MODULE
#if DMA3_P6
#else
	if (spi_device) {
		device_del(&spi_device->dev);
		kfree(spi_device);
	}
#endif
//#endif
	spi_unregister_driver(&dm9051_driver);
}

module_exit(dm9051_cleanup);

MODULE_DESCRIPTION("Davicom DM9051 network driver");
MODULE_AUTHOR("Joseph CHANG <joseph_chang@davicom.com.tw>");
MODULE_LICENSE("GPL");
module_param_named(debug, debug.msg_enable, int, 0);
MODULE_PARM_DESC(debug, "Debug verbosity level (0=none, ..., ffff=all)");
MODULE_ALIAS("spi:dm9051");

