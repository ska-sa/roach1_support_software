/*
 * Copyright(c) 2006 DENX Engineering. All rights reserved.
 *
 * Author: Yuri Tikhonov <yur@emcraft.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The full GNU General Public License is included in this distribution in the
 * file called COPYING.
 */

/*
 *  This driver supports the asynchrounous DMA copy and RAID engines available
 * on the AMCC PPC440SPe Processors.
 *  Based on the Intel Xscale(R) family of I/O Processors (IOP 32x, 33x, 134x)
 * ADMA driver written by D.Williams.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <asm-ppc/ibm44x.h>
#include "ppc440epx_dma3.h"

#include <asm/io.h>

#define DRIVER_NAME "ppc440epx_dma3"

#define DBG(x...) \
  pr_debug(DRIVER_NAME ": " x)
#define DBGF(f, x...) \
  pr_debug(DRIVER_NAME " [%s()]: " f, __func__ , ##x)

//CPR_READ(a)
//CPR_WRITE(a,d)

/*
 * Module loading/unloading
 */

void *base = NULL;

static int __devinit ppc440epx_dma3_probe(struct platform_device *dev)
{
  unsigned int val = 0xdeadbeef;
  unsigned int *buf;
  int tick=0;

  if (!request_mem_region(0xE0010000, 0x2000, "roach-cpld-mmc")) {
    printk(KERN_ERR "%s: memory range 0x%08x to 0x%08x is in use\n",
                          "roach-cpld",
                          (resource_size_t) 0xE0010000,
                          (resource_size_t)(0xE0010000 + 0x2000));
    return ENOMEM;
  }

  base = ioremap64(0xE0010000, 0x2000);
  printk("rmmc: ioremap - req = %x, ret = %x\n", 0xE0010000, (unsigned int) base);
  if (!(base)) {
    release_mem_region(0xE0010000, 0x2000);
    return ENOMEM;
  }

  buf = (unsigned int*)(base);
  buf[0x100] = 0;
  buf[0x101] = 1;
  buf[0x102] = 2;

  buf[0] = 0xdeadbeef;
  

  printk("dump0: %x %x %x\n", buf[0x100], buf[0x101], buf[0x102]);

  DMA_WRITE(DMA2P30_SR,  0);
  DMA_WRITE(DMA2P30_CR0, 0);
  DMA_WRITE(DMA2P30_SG0, 0);
  DMA_WRITE(DMA2P30_SC0, 0);
  DMA_WRITE(DMA2P30_SA0, 0);
  DMA_WRITE(DMA2P30_DA0, 0);
  DMA_WRITE(DMA2P30_CT0, 0);
  DMA_WRITE(DMA2P30_ADR, 0);
  DMA_WRITE(DMA2P30_SLP, 0);
  DMA_WRITE(DMA2P30_POL, 0);

  printk("ppc440epx_dma3: DMA2P30_SR  = %x\n", DMA_READ(DMA2P30_SR));
  printk("ppc440epx_dma3: DMA2P30_SGC = %x\n", DMA_READ(DMA2P30_SGC));
  printk("ppc440epx_dma3: DMA2P30_ADR = %x\n", DMA_READ(DMA2P30_ADR));
  printk("ppc440epx_dma3: DMA2P30_SLP = %x\n", DMA_READ(DMA2P30_SLP));
  printk("ppc440epx_dma3: DMA2P30_POL = %x\n", DMA_READ(DMA2P30_POL));


  DMA_WRITE(DMA2P30_SA0, 0xE0010000);
  DMA_WRITE(DMA2P30_DA0, 0xE0010400);
  DMA_WRITE(DMA2P30_CT0, 100);

  printk("ppc440epx_dma3: DMA2P30_CR0 = %x\n", DMA_READ(DMA2P30_CR0));
  printk("ppc440epx_dma3: DMA2P30_SA0 = %x\n", DMA_READ(DMA2P30_SA0));
  printk("ppc440epx_dma3: DMA2P30_DA0 = %x\n", DMA_READ(DMA2P30_DA0));
  printk("ppc440epx_dma3: DMA2P30_CT0 = %x\n", DMA_READ(DMA2P30_CT0));

  DMA_WRITE(DMA2P30_CR0, (DMA2P3_CE | DMA2P3_DIA | DMA2P3_BEN | DMA2P3_TM_SMM | DMA2P3_PW_W | DMA2P3_ETD| DMA2P3_TCE));

  while (DMA_READ(DMA2P30_CT0)){
    tick++;
    if (tick == 8000)
      break;
  }
  printk("ticks: %d.\n",tick);
  printk("ppc440epx_dma3: DMA2P30_CR0 = %x\n", DMA_READ(DMA2P30_CR0));

  printk("dump1: %x %x %x\n", buf[0x100], buf[0x101], buf[0x102]);
  printk("ppc440epx_dma3: DMA2P30_SR  = %x\n", DMA_READ(DMA2P30_SR));
  printk("ppc440epx_dma3: DMA2P30_CT0  = %x\n", DMA_READ(DMA2P30_CT0));

  return 0;
}

static int __devexit ppc440epx_dma3_remove(struct platform_device *dev)
{
  release_mem_region(0xE0010000, 0x2000);
  iounmap(base);
  return 0;
}

static struct platform_device *ppc440epx_dma3_device;

static struct platform_driver ppc440epx_dma3_driver = {
  .probe    = ppc440epx_dma3_probe,
  .remove   = __devexit_p(ppc440epx_dma3_remove),

  .suspend  = NULL,
  .resume   = NULL,
  .driver   = {
    .name  = DRIVER_NAME,
    .owner = THIS_MODULE,
  },
};

static int __init ppc440epx_dma3_init(void)
{
  int result;
  printk(KERN_INFO DRIVER_NAME ": PPC440EPx-PLB3-DMA dma-engine driver\n");

  result = platform_driver_register(&ppc440epx_dma3_driver);
  if (result){
    printk(KERN_INFO DRIVER_NAME ": error during platform_driver_register\n");
    return result;
  }

  ppc440epx_dma3_device = platform_device_alloc(DRIVER_NAME, -1);
  if (!ppc440epx_dma3_device) {
    platform_driver_unregister(&ppc440epx_dma3_driver);
    return -ENOMEM;
  }

  result = platform_device_add(ppc440epx_dma3_device);
  if (result) {
    platform_device_put(ppc440epx_dma3_device);
    platform_driver_unregister(&ppc440epx_dma3_driver);
    return result;
  }

  return 0;
}

static void __exit ppc440epx_dma3_exit(void)
{
  platform_device_unregister(ppc440epx_dma3_device);
  platform_driver_unregister(&ppc440epx_dma3_driver);
  DBG("unloaded\n");
  return;
}

module_init(ppc440epx_dma3_init);
module_exit(ppc440epx_dma3_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("David George");
MODULE_DESCRIPTION("PPC440EPx-PLB3-DMA");

MODULE_PARM_DESC(foo, "A dummy parameter");
