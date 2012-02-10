/*
 * PPC440SP & PPC440SPE DMA engines description
 *
 * Yuri Tikhonov <yur@emcraft.com>
 * Copyright (c) 2007 DENX Engineering.  All rights reserved.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/async_tx.h>
#include <linux/platform_device.h>

#include <asm/ppc440spe_adma.h>
#include <asm/dcr.h>
#include <asm/dcr-regs.h>

static u64 ppc440spe_adma_dmamask = DMA_32BIT_MASK;

/* DMA and XOR platform devices' resources */
static struct resource ppc440spe_dma_0_resources[] = {
	{
		.flags = IORESOURCE_MEM,
		.start = DMA0_MMAP_BASE,
		.end = DMA0_MMAP_BASE + DMA_MMAP_SIZE - 1
	}
};

static struct resource ppc440spe_dma_1_resources[] = {
	{
		.flags = IORESOURCE_MEM,
		.start = DMA1_MMAP_BASE,
		.end = DMA1_MMAP_BASE + DMA_MMAP_SIZE - 1
	}
};

static struct resource ppc440spe_xor_resources[] = {
	{
		.flags = IORESOURCE_MEM,
		.start = XOR_MMAP_BASE,
		.end = XOR_MMAP_BASE + XOR_MMAP_SIZE -1
	}
};

/* DMA and XOR platform devices' data */

/* DMA0,1 engines use FIFO to maintain CDBs, so we
 * should allocate the pool accordingly to size of this
 * FIFO. Thus, the pool size depends on the FIFO depth:
 * how much CDBs pointers FIFO may contaun then so much
 * CDBs we should provide in pool.
 * That is
 *   CDB size = 32B;
 *   CDBs number = (DMA0_FIFO_SIZE >> 3);
 *   Pool size = CDBs number * CDB size =
 *      = (DMA0_FIFO_SIZE >> 3) << 5 = DMA0_FIFO_SIZE << 2.
 *
 *  As far as the XOR engine is concerned, it does not
 * use FIFOs but uses linked list. So there is no dependency
 * between pool size to allocate and the engine configuration.
 */
static struct ppc440spe_adma_platform_data ppc440spe_dma_0_data = {
	.hw_id  = PPC440SPE_DMA0_ID,
	.pool_size = DMA0_FIFO_SIZE << 2,
};

static struct ppc440spe_adma_platform_data ppc440spe_dma_1_data = {
	.hw_id  = PPC440SPE_DMA1_ID,
	.pool_size = DMA0_FIFO_SIZE << 2,
};

static struct ppc440spe_adma_platform_data ppc440spe_xor_data = {
	.hw_id  = PPC440SPE_XOR_ID,
	.pool_size = PAGE_SIZE << 1,
};

/* DMA and XOR platform devices definitions */
static struct platform_device ppc440spe_dma_0_channel = {
	.name = "PPC440SP(E)-ADMA",
	.id = PPC440SPE_DMA0_ID,
	.num_resources = ARRAY_SIZE(ppc440spe_dma_0_resources),
	.resource = ppc440spe_dma_0_resources,
	.dev = {
		.dma_mask = &ppc440spe_adma_dmamask,
		.coherent_dma_mask = DMA_64BIT_MASK,
		.platform_data = (void *) &ppc440spe_dma_0_data,
	},
};

static struct platform_device ppc440spe_dma_1_channel = {
	.name = "PPC440SP(E)-ADMA",
	.id = PPC440SPE_DMA1_ID,
	.num_resources = ARRAY_SIZE(ppc440spe_dma_1_resources),
	.resource = ppc440spe_dma_1_resources,
	.dev = {
		.dma_mask = &ppc440spe_adma_dmamask,
		.coherent_dma_mask = DMA_64BIT_MASK,
		.platform_data = (void *) &ppc440spe_dma_1_data,
	},
};

static struct platform_device ppc440spe_xor_channel = {
	.name = "PPC440SP(E)-ADMA",
	.id = PPC440SPE_XOR_ID,
	.num_resources = ARRAY_SIZE(ppc440spe_xor_resources),
	.resource = ppc440spe_xor_resources,
	.dev = {
		.dma_mask = &ppc440spe_adma_dmamask,
		.coherent_dma_mask = DMA_64BIT_MASK,
		.platform_data = (void *) &ppc440spe_xor_data,
	},
};

/*
 *  Init DMA0/1 and XOR engines; allocate memory for DMAx FIFOs; set platform_device
 * memory resources addresses
 */
static void ppc440spe_configure_raid_devices(void)
{
	void *fifo_buf;
	volatile i2o_regs_t *i2o_reg;
	volatile dma_regs_t *dma_reg0, *dma_reg1;
	volatile xor_regs_t *xor_reg;
	u32 mask;

	/*
	 * Map registers and allocate fifo buffer
	 */
	if (!(i2o_reg  = ioremap(I2O_MMAP_BASE, I2O_MMAP_SIZE))) {
		printk(KERN_ERR "I2O registers mapping failed.\n");
		return;
	}
	if (!(dma_reg0 = ioremap(DMA0_MMAP_BASE, DMA_MMAP_SIZE))) {
		printk(KERN_ERR "DMA0 registers mapping failed.\n");
		goto err1;
	}
	if (!(dma_reg1 = ioremap(DMA1_MMAP_BASE, DMA_MMAP_SIZE))) {
		printk(KERN_ERR "DMA1 registers mapping failed.\n");
		goto err2;
	}
	if (!(xor_reg  = ioremap(XOR_MMAP_BASE,XOR_MMAP_SIZE))) {
		printk(KERN_ERR "XOR registers mapping failed.\n");
		goto err3;
	}

	/*  Provide memory regions for DMA's FIFOs: I2O, DMA0 and DMA1 share
	 * the base address of FIFO memory space.
	 *  Actually we need twice more physical memory than programmed in the
	 * <fsiz> register (because there are two FIFOs foreach DMA: CP and CS)
	 */
	fifo_buf = kmalloc((DMA0_FIFO_SIZE + DMA1_FIFO_SIZE)<<1, GFP_KERNEL);
	if (!fifo_buf) {
		printk(KERN_ERR "DMA FIFO buffer allocating failed.\n");
		goto err4;
	}

	/*
	 * Configure h/w
	 */
	/* Reset I2O/DMA */
	mtdcri(SDR, DCRN_SDR_SRST, DCRN_SDR_SRST_I2ODMA);
	mtdcri(SDR, DCRN_SDR_SRST, 0);

	/* Reset XOR */
	xor_reg->crsr = XOR_CRSR_XASR_BIT;
	xor_reg->crrr = XOR_CRSR_64BA_BIT;

	/* Setup the base address of mmaped registers */
	mtdcr(DCRN_I2O0_IBAH, (u32)(I2O_MMAP_BASE >> 32));
	mtdcr(DCRN_I2O0_IBAL, (u32)(I2O_MMAP_BASE) | I2O_REG_ENABLE);

	/* SetUp FIFO memory space base address */
	out_le32(&i2o_reg->ifbah, 0);
	out_le32(&i2o_reg->ifbal, ((u32)__pa(fifo_buf)));

	/* set zero FIFO size for I2O, so the whole fifo_buf is used by DMAs.
	 * DMA0_FIFO_SIZE is defined in bytes, <fsiz> - in number of CDB pointers (8byte).
	 * DMA FIFO Length = CSlength + CPlength, where
	 *  CSlength = CPlength = (fsiz + 1) * 8.
	 */
	out_le32(&i2o_reg->ifsiz, 0);
	out_le32(&dma_reg0->fsiz, DMA_FIFO_ENABLE | ((DMA0_FIFO_SIZE>>3) - 2));
	out_le32(&dma_reg1->fsiz, DMA_FIFO_ENABLE | ((DMA1_FIFO_SIZE>>3) - 2));
	/* Configure DMA engine */
	out_le32(&dma_reg0->cfg, DMA_CFG_DXEPR_HP | DMA_CFG_DFMPP_HP | DMA_CFG_FALGN);
	out_le32(&dma_reg1->cfg, DMA_CFG_DXEPR_HP | DMA_CFG_DFMPP_HP | DMA_CFG_FALGN);

	/* Clear Status */
	out_le32(&dma_reg0->dsts, ~0);
	out_le32(&dma_reg1->dsts, ~0);

	/*
	 * Prepare WXOR/RXOR (finally it is being enabled via /proc interface of
	 * the ppc440spe ADMA driver)
	 */
	/* Set HB alias */
	mtdcr(DCRN_MQ0_BAUH, DMA_CUED_XOR_HB);

	/* Set:
	 * - LL transaction passing limit to 1;
	 * - Memory controller cycle limit to 1;
	 * - Galois Polynomial to 0x14d (default)
	 */
	mtdcr(DCRN_MQ0_CFBHL, (1 << MQ0_CFBHL_TPLM) |
			      (1 << MQ0_CFBHL_HBCL) |
			      (PPC440SPE_DEFAULT_POLY << MQ0_CFBHL_POLY));

	/* Unmask 'CS FIFO Attention' interrupts and
	 * enable generating interrupts on errors
	 */
	mask = in_le32(&i2o_reg->iopim) & ~(
		I2O_IOPIM_P0SNE | I2O_IOPIM_P1SNE |
		I2O_IOPIM_P0EM | I2O_IOPIM_P1EM);
	out_le32(&i2o_reg->iopim, mask);

	/* enable XOR engine interrupts */
	xor_reg->ier = XOR_IE_CBCIE_BIT |
		 XOR_IE_ICBIE_BIT | XOR_IE_ICIE_BIT | XOR_IE_RPTIE_BIT;

	/*
	 * Unmap registers
	 */
	iounmap(i2o_reg);
	iounmap(xor_reg);
	iounmap(dma_reg1);
	iounmap(dma_reg0);

	/*
	 * Set resource addresses
	 */
	dma_cap_set(DMA_MEMCPY, ppc440spe_dma_0_data.cap_mask);
	dma_cap_set(DMA_INTERRUPT, ppc440spe_dma_0_data.cap_mask);
	dma_cap_set(DMA_MEMSET, ppc440spe_dma_0_data.cap_mask);
	dma_cap_set(DMA_PQ, ppc440spe_dma_0_data.cap_mask);
	dma_cap_set(DMA_PQ_ZERO_SUM, ppc440spe_dma_0_data.cap_mask);

	dma_cap_set(DMA_MEMCPY, ppc440spe_dma_1_data.cap_mask);
	dma_cap_set(DMA_INTERRUPT, ppc440spe_dma_1_data.cap_mask);
	dma_cap_set(DMA_MEMSET, ppc440spe_dma_1_data.cap_mask);
	dma_cap_set(DMA_PQ, ppc440spe_dma_1_data.cap_mask);
	dma_cap_set(DMA_PQ_ZERO_SUM, ppc440spe_dma_1_data.cap_mask);

	dma_cap_set(DMA_XOR, ppc440spe_xor_data.cap_mask);
	dma_cap_set(DMA_PQ, ppc440spe_xor_data.cap_mask);
	dma_cap_set(DMA_INTERRUPT, ppc440spe_xor_data.cap_mask);

	return;
err4:
	iounmap(xor_reg);
err3:
	iounmap(dma_reg1);
err2:
	iounmap(dma_reg0);
err1:
	iounmap(i2o_reg);
	return;
}

static struct platform_device *ppc440spe_devs[] __initdata = {
	&ppc440spe_dma_0_channel,
	&ppc440spe_dma_1_channel,
	&ppc440spe_xor_channel,
};

static int __init ppc440spe_register_raid_devices(void)
{
	ppc440spe_configure_raid_devices();
	platform_add_devices(ppc440spe_devs, ARRAY_SIZE(ppc440spe_devs));

	return 0;
}

arch_initcall(ppc440spe_register_raid_devices);
