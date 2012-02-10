/*
 *  linux/drivers/mmc/host/mmc_dummy.c - Winbond W83L51xD SD/MMC driver
 *
 *  Copyright (C) 2004-2007 Pierre Ossman, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 *
 * Warning!
 *
 * Changes to the FIFO system should be done with extreme care since
 * the hardware is full of bugs related to the FIFO. Known issues are:
 *
 * - FIFO size field in FSR is always zero.
 *
 * - FIFO interrupts tend not to work as they should. Interrupts are
 *   triggered only for full/empty events, not for threshold values.
 *
 * - On APIC systems the FIFO empty interrupt is sometimes lost.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/pnp.h>
#include <linux/highmem.h>
#include <linux/mmc/host.h>
#include <linux/scatterlist.h>
#include <linux/mmc/mmc.h>

#include <asm/io.h>
#include <asm/dma.h>

#include "mmc_dummy.h"


#define DRIVER_NAME "mmc_dummy"

#define DBG(x...) \
	pr_debug(DRIVER_NAME ": " x)
#define DBGF(f, x...) \
	pr_debug(DRIVER_NAME " [%s()]: " f, __func__ , ##x)


/*
 * Common routines
 */

static void mmc_dummy_init_device(struct mmc_dummy_host *host)
{
  printk("mmc_dummy: mmc host init\n");
  host->flags &= ~WBSD_FIGNORE_DETECT;
  host->clk = 0x10;
  /*
   * Test for card presence
  */
  if (1)
    host->flags |= WBSD_FCARD_PRESENT;
  else
    host->flags &= ~WBSD_FCARD_PRESENT;
}

static void mmc_dummy_request_end(struct mmc_dummy_host *host, struct mmc_request *mrq)
{
  printk("mmc_dummy: request end, disabling dma\n");

	host->mrq = NULL;

	/*
	 * MMC layer might call back into the driver so first unlock.
	 */
	spin_unlock(&host->lock);
	mmc_request_done(host->mmc, mrq);
	spin_lock(&host->lock);
}


static void mmc_dummy_free_mmc(struct device *dev)
{
	struct mmc_host *mmc;
	struct mmc_dummy_host *host;

	mmc = dev_get_drvdata(dev);
	if (!mmc)
		return;

	host = mmc_priv(mmc);
	BUG_ON(host == NULL);

	del_timer_sync(&host->ignore_timer);

	mmc_free_host(mmc);

	dev_set_drvdata(dev, NULL);
}

/*
 * Scan for known chip id:s
 */

static int __devinit mmc_dummy_scan(struct mmc_dummy_host *host)
{
	/*
	 * Check for device presence
	 */

	if (1) {
	  return 0;
  }

	return -ENODEV;
}


/*
 * Allocate all resources for the host.
 */

static int __devinit mmc_dummy_request_resources(struct mmc_dummy_host *host)
{
  if (1)
    return 0;
  return ENOMEM;
}

/*
 * Release all resources for the host.
 */

static void mmc_dummy_release_resources(struct mmc_dummy_host *host)
{
}

/*
 * Configure the resources the chip should use.
 */

static void mmc_dummy_chip_config(struct mmc_dummy_host *host)
{
  /* configure the chip */
}

/*****************************************************************************\
 *                                                                           *
 * MMC layer callbacks                                                       *
 *                                                                           *
\*****************************************************************************/



#define MMC_STATE_IDLE    0
#define MMC_STATE_READY   1
#define MMC_STATE_IDENT   2
#define MMC_STATE_STANDBY 3
#define MMC_STATE_XFER    4
#define MMC_STATE_SEND    5

unsigned int state = MMC_STATE_IDLE;

unsigned int mmc_sim_process(u32 cmd, u32 *resp)
{
  int ret = 0;
  printk("foo - s %d - c %d\n", state, cmd);
  if (cmd == 0) {
    state = MMC_STATE_IDLE;
    return 0;
  }
  if (cmd == 13) {
    resp[0] = 0x0 | ((state) << 9) | (R1_READY_FOR_DATA);
	return 0;
  }

   
  switch (state){
    case MMC_STATE_IDLE:
      if (cmd == 1) {
        state = MMC_STATE_READY; //ocr
        resp[0] = 0x80ff8080;
      } else {
        ret = 1;
      }
      break;
    case MMC_STATE_READY:
      if (cmd == 2) {
        state = MMC_STATE_IDENT;
      } else {
        ret = 1;
      }
      break;
    case MMC_STATE_IDENT:
      if (cmd == 3) {
        state = MMC_STATE_STANDBY;
      } else {
        ret = 1;
      }
      break;
    case MMC_STATE_STANDBY:
      if (cmd == 7) {
        state = MMC_STATE_XFER;
      } else if (cmd == 9) { //csd
        resp[3] = 0x90000000;
        resp[2] = 0x00000002;
        resp[1] = 0xfff00003;
        resp[0] = 0x90ff8052; //MSbyte
      } else {
        ret = 1;
      }
      break;
    case MMC_STATE_XFER:
      if (cmd == 6 || cmd == 16 || cmd == 8 || cmd == 24 || cmd == 25) {
      } else if (cmd == 18 || cmd == 17) {
        //state = MMC_STATE_SEND;
      } else {
        ret = 1;
      }
      break;
	case MMC_STATE_SEND:
      if (cmd == 12) {
        state = MMC_STATE_XFER;
      } else if (cmd == 7) {
        state = MMC_STATE_STANDBY;
      } else {
        ret = 1;
      }
      break;
    default:
      ret = 1;
      break;
  }
  return ret;
}

static inline char *sg_to_buffer(struct scatterlist* sl)
{
  return sg_virt(sl);
}


void mmc_sim_read(u32 cmd, struct mmc_data *mmc_data)
{
  int len;
  char *data;
  if (mmc_data->sg_len == 0)
    return;
  if (!mmc_data->sg)
    return;

  data=sg_to_buffer(mmc_data->sg);
  len = mmc_data->sg->length;

  switch (cmd){
    case 8: 
      data[EXT_CSD_REV]= 0x02;
      data[EXT_CSD_SEC_CNT + 0]= 0xff;
      data[EXT_CSD_SEC_CNT + 1]= 0xff;
      data[EXT_CSD_SEC_CNT + 2]= 0x0f;
      data[EXT_CSD_CARD_TYPE]=EXT_CSD_CARD_TYPE_52 | EXT_CSD_CARD_TYPE_26;
      break;
  }
  mmc_data->bytes_xfered = mmc_data->blksz * mmc_data->blocks;
  return;
}

void mmc_sim_write(u32 cmd, struct mmc_data *mmc_data)
{
  mmc_data->bytes_xfered = mmc_data->blksz * mmc_data->blocks;
  return;
}

void dump_scatterlist(int len,  struct scatterlist* sl)
{
  int i;
  struct scatterlist* curr = sl;
  for (i = 0; i < len; i++){
    if (!curr){
      return;
    }
    printk(" sl_%d: l=%d\n",i,curr->length);
    curr++;
  }
}

static void mmc_dummy_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct mmc_dummy_host *host = mmc_priv(mmc);
	struct mmc_command *cmd;

	/*
	 * Disable tasklets to avoid a deadlock.
	 */

	spin_lock_bh(&host->lock);

	BUG_ON(host->mrq != NULL);

	cmd = mrq->cmd;

	host->mrq = mrq;

  printk("mmc_dummy: processing command\n");
  printk("           op = %d, arg = %d, flags = %d\n", cmd->opcode, cmd->arg, cmd->flags);

	/*
	 * Check that there is actually a card in the slot.
	 */
  if (mmc_sim_process(cmd->opcode, cmd->resp)) {
    cmd->error = -ETIMEDOUT;
  }

  if (cmd->data && !cmd->error){
    if (cmd->data->flags & MMC_DATA_WRITE) {
      mmc_sim_write(cmd->opcode, cmd->data);
    } else if (cmd->data->flags & MMC_DATA_READ){
      //printk("mmc_dummy: read req - sq_len = %d, \n", cmd->data->sg_len);
      //dump_scatterlist(cmd->data->sg_len, cmd->data->sg);
      mmc_sim_read(cmd->opcode, cmd->data);
    }
  }

	mmc_dummy_request_end(host, mrq);

	spin_unlock_bh(&host->lock);
}

static void mmc_dummy_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct mmc_dummy_host *host = mmc_priv(mmc);
  u8 clk;
  printk("mmc_dummy: set_ios?\n");

	spin_lock_bh(&host->lock);

	/*
	 * Reset the chip on each power off.
	 * Should clear out any weird states.
	 */
	if (ios->power_mode == MMC_POWER_OFF)
		mmc_dummy_init_device(host);

	if (ios->clock >= 24000000)
		clk = WBSD_CLK_24M;
	else if (ios->clock >= 16000000)
		clk = WBSD_CLK_16M;
	else if (ios->clock >= 12000000)
		clk = WBSD_CLK_12M;
	else
		clk = WBSD_CLK_375K;

	/*
	 * Only write to the clock register when
	 * there is an actual change.
	 */
	if (clk != host->clk) {
		host->clk = clk;
	}

	/*
	 * Power up card.
	 */

	if (ios->power_mode != MMC_POWER_OFF) {
	}

	/*
	 * MMC cards need to have pin 1 high during init.
	 * It wreaks havoc with the card detection though so
	 * that needs to be disabled.
	 */

	/*
	 * Store bus width for later. Will be used when
	 * setting up the data transfer.
	 */

	host->bus_width = ios->bus_width;

	spin_unlock_bh(&host->lock);
}

static int mmc_dummy_get_ro(struct mmc_host *mmc)
{
	return 0;
}

static int mmc_dummy_get_cd(struct mmc_host *mmc)
{
	return 1;
}

static const struct mmc_host_ops mmc_dummy_ops = {
	.request	= mmc_dummy_request,
	.set_ios	= mmc_dummy_set_ios,
	.get_ro		= mmc_dummy_get_ro
	//.get_cd		= mmc_dummy_get_cd
};


static unsigned int param_foo = 0x666;

/********************** Misc ***********************/

static int __devinit mmc_dummy_alloc_mmc(struct device *dev)
{
	struct mmc_host *mmc;
	struct mmc_dummy_host *host;

	/*
	 * Allocate MMC structure.
	 */
	mmc = mmc_alloc_host(sizeof(struct mmc_dummy_host), dev);
	if (!mmc)
		return -ENOMEM;

	host = mmc_priv(mmc); /* TODO: what is going on here? */
	host->mmc = mmc;

	host->dma = -1;

	/*
	 * Set host parameters.
	 */
	mmc->ops = &mmc_dummy_ops;
	mmc->f_min = 375000;
	mmc->f_max = 24000000;
	mmc->ocr_avail = MMC_VDD_32_33 | MMC_VDD_33_34;
	mmc->caps = MMC_CAP_4_BIT_DATA;

	spin_lock_init(&host->lock);

	/*
	 * Set up timers
	 */
	init_timer(&host->ignore_timer);
	host->ignore_timer.data = (unsigned long)host;
	//host->ignore_timer.function = mmc_dummy_reset_ignore;

	/*
	 * Maximum number of segments. Worst case is one sector per segment
	 * so this will be 64kB/512.
	 */
	mmc->max_hw_segs = 128;
	mmc->max_phys_segs = 128;

	/*
	 * Maximum request size. Also limited by 64KiB buffer.
	 */
	mmc->max_req_size = 65536;

	/*
	 * Maximum segment size. Could be one segment with the maximum number
	 * of bytes.
	 */
	mmc->max_seg_size = mmc->max_req_size;

	/*
	 * Maximum block size. We have 12 bits (= 4095) but have to subtract
	 * space for CRC. So the maximum is 4095 - 4*2 = 4087.
	 */
	mmc->max_blk_size = 4087;

	/*
	 * Maximum block count. There is no real limit so the maximum
	 * request size will be the only restriction.
	 */
	mmc->max_blk_count = mmc->max_req_size;

	dev_set_drvdata(dev, mmc);

	return 0;
}


/*****************************************************************************\
 *                                                                           *
 * Devices setup and shutdown                                                *
 *                                                                           *
\*****************************************************************************/


static int __devinit mmc_dummy_init(struct device *dev)
{
	struct mmc_dummy_host *host = NULL;
	struct mmc_host *mmc = NULL;
	int ret;

	ret = mmc_dummy_alloc_mmc(dev);
	if (ret)
		return ret;

	mmc = dev_get_drvdata(dev);
	host = mmc_priv(mmc);

	/*
	 * Scan for hardware.
	 */
	ret = mmc_dummy_scan(host);
	if (ret) { /* no device found, exit */
		mmc_dummy_free_mmc(dev);
		return ret;
	}

	/*
	 * Request resources.
	 */
	ret = mmc_dummy_request_resources(host);
	if (ret) {
		mmc_dummy_release_resources(host);
		mmc_dummy_free_mmc(dev);
		return ret;
	}

	/*
	 * See if chip needs to be configured.
	 */
	mmc_dummy_chip_config(host);

	/*
	 * Allow device to initialise itself properly.
	 */
	mdelay(5);

	/*
	 * Reset the chip into a known state.
	 */
	mmc_dummy_init_device(host);

	mmc_add_host(mmc);

	printk(KERN_INFO "mmc_dummy: initialized mmc\n");

	return 0;
}


static int __devinit mmc_dummy_probe(struct platform_device *dev)
{
	/* Use the module parameters for resources */
  return mmc_dummy_init(&dev->dev);
}

static int __devexit mmc_dummy_remove(struct platform_device *dev)
{
  struct mmc_host *mmc = dev_get_drvdata(&dev->dev);
  struct mmc_dummy_host *host;
  if (!mmc)
    return 0;

  host = mmc_priv(mmc);

  mmc_remove_host(mmc);

  BUG_ON(host == NULL);

  del_timer_sync(&host->ignore_timer);

  mmc_free_host(mmc);

  dev_set_drvdata(&dev->dev, NULL);

	return 0;
}

static struct platform_device *mmc_dummy_device;

static struct platform_driver mmc_dummy_driver = {
	.probe		= mmc_dummy_probe,
	.remove		= __devexit_p(mmc_dummy_remove),

	.suspend	= NULL,
	.resume		= NULL,
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
};

/*
 * Module loading/unloading
 */

static int __init mmc_dummy_drv_init(void)
{
	int result;

	printk(KERN_INFO DRIVER_NAME ": Dummy MMC driver\n");

	result = platform_driver_register(&mmc_dummy_driver);
	if (result < 0)
		return result;

	mmc_dummy_device = platform_device_alloc(DRIVER_NAME, -1);
	if (!mmc_dummy_device) {
		platform_driver_unregister(&mmc_dummy_driver);
		return -ENOMEM;
	}

	result = platform_device_add(mmc_dummy_device);
	if (result) {
		platform_device_put(mmc_dummy_device);
		platform_driver_unregister(&mmc_dummy_driver);
		return result;
	}

	return 0;
}

static void __exit mmc_dummy_drv_exit(void)
{
	platform_device_unregister(mmc_dummy_device);
	platform_driver_unregister(&mmc_dummy_driver);
	DBG("unloaded\n");
}

module_init(mmc_dummy_drv_init);
module_exit(mmc_dummy_drv_exit);
module_param_named(foo, param_foo, uint, 0444);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Shanly Rajan and David George");
MODULE_DESCRIPTION("Dummy MMC interface");

MODULE_PARM_DESC(foo, "A dummy parameter");
