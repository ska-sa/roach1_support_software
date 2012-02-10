/*
 * PPC440SPe I/O descriptions
 *
 * Roland Dreier <rolandd@cisco.com>
 * Copyright (c) 2005 Cisco Systems.  All rights reserved.
 *
 * Matt Porter <mporter@kernel.crashing.org>
 * Copyright 2002-2005 MontaVista Software Inc.
 *
 * Eugene Surovegin <eugene.surovegin@zultys.com> or <ebs@ebshome.net>
 * Copyright (c) 2003, 2004 Zultys Technologies
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <platforms/4xx/ppc440spe.h>
#include <asm/ocp.h>
#include <asm/ppc4xx_pic.h>

#if defined(CONFIG_AMCC_PPC440SPE_ADMA)
#include <syslib/ppc440spe_pcie.h>
#include <linux/async_tx.h>
#include <linux/platform_device.h>
#include <asm/ppc440spe_adma.h>
#endif

static struct ocp_func_emac_data ppc440spe_emac0_def = {
	.rgmii_idx	= -1,		/* No RGMII */
	.rgmii_mux	= -1,		/* No RGMII */
	.zmii_idx       = -1,           /* No ZMII */
	.zmii_mux       = -1,           /* No ZMII */
	.mal_idx        = 0,            /* MAL device index */
	.mal_rx_chan    = 0,            /* MAL rx channel number */
	.mal_tx_chan    = 0,            /* MAL tx channel number */
	.wol_irq        = 61,  		/* WOL interrupt number */
	.mdio_idx       = -1,           /* No shared MDIO */
	.tah_idx	= -1,		/* No TAH */
};
OCP_SYSFS_EMAC_DATA()

static struct ocp_func_mal_data ppc440spe_mal0_def = {
	.num_tx_chans   = 1,    	/* Number of TX channels */
	.num_rx_chans   = 1,    	/* Number of RX channels */
	.txeob_irq	= 38,		/* TX End Of Buffer IRQ  */
	.rxeob_irq	= 39,		/* RX End Of Buffer IRQ  */
	.txde_irq	= 34,		/* TX Descriptor Error IRQ */
	.rxde_irq	= 35,		/* RX Descriptor Error IRQ */
	.serr_irq	= 33,		/* MAL System Error IRQ    */
	.dcr_base	= DCRN_MAL_BASE /* MAL0_CFG DCR number */
};
OCP_SYSFS_MAL_DATA()

static struct ocp_func_iic_data ppc440spe_iic0_def = {
	.fast_mode	= 0,		/* Use standad mode (100Khz) */
};

static struct ocp_func_iic_data ppc440spe_iic1_def = {
	.fast_mode	= 0,		/* Use standad mode (100Khz) */
};
OCP_SYSFS_IIC_DATA()

struct ocp_def core_ocp[] = {
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_16550,
	  .index	= 0,
	  .paddr	= PPC440SPE_UART0_ADDR,
	  .irq		= UART0_INT,
	  .pm		= IBM_CPM_UART0,
	},
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_16550,
	  .index	= 1,
	  .paddr	= PPC440SPE_UART1_ADDR,
	  .irq		= UART1_INT,
	  .pm		= IBM_CPM_UART1,
	},
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_16550,
	  .index	= 2,
	  .paddr	= PPC440SPE_UART2_ADDR,
	  .irq		= UART2_INT,
	  .pm		= IBM_CPM_UART2,
	},
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_IIC,
	  .index	= 0,
	  .paddr	= 0x00000004f0000400ULL,
	  .irq		= 2,
	  .pm		= IBM_CPM_IIC0,
	  .additions	= &ppc440spe_iic0_def,
	  .show		= &ocp_show_iic_data
	},
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_IIC,
	  .index	= 1,
	  .paddr	= 0x00000004f0000500ULL,
	  .irq		= 3,
	  .pm		= IBM_CPM_IIC1,
	  .additions	= &ppc440spe_iic1_def,
	  .show		= &ocp_show_iic_data
	},
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_GPIO,
	  .index	= 0,
	  .paddr	= 0x00000004f0000700ULL,
	  .irq		= OCP_IRQ_NA,
	  .pm		= IBM_CPM_GPIO0,
	},
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_MAL,
	  .paddr	= OCP_PADDR_NA,
	  .irq		= OCP_IRQ_NA,
	  .pm		= OCP_CPM_NA,
	  .additions	= &ppc440spe_mal0_def,
	  .show		= &ocp_show_mal_data,
	},
	{ .vendor	= OCP_VENDOR_IBM,
	  .function	= OCP_FUNC_EMAC,
	  .index	= 0,
	  .paddr	= 0x00000004f0000800ULL,
	  .irq		= 60,
	  .pm		= OCP_CPM_NA,
	  .additions	= &ppc440spe_emac0_def,
	  .show		= &ocp_show_emac_data,
	},
	{ .vendor	= OCP_VENDOR_INVALID
	}
};

/* Polarity and triggering settings for internal interrupt sources */
struct ppc4xx_uic_settings ppc4xx_core_uic_cfg[] __initdata = {
	{ .polarity     = 0xffffffff,
	  .triggering   = 0x010f0004,
	  .ext_irq_mask = 0x00000000,
	},
	{ .polarity     = 0xffffffff,
	  .triggering   = 0x001f8040,
	  .ext_irq_mask = 0x00007c30,   /* IRQ6 - IRQ7, IRQ8 - IRQ12 */
	},
	{ .polarity     = 0xffffffff,
	  .triggering   = 0x00000000,
	  .ext_irq_mask = 0x000000fc,   /* IRQ0 - IRQ5 */
	},
	{ .polarity     = 0xffffffff,
	  .triggering   = 0x00000000,
	  .ext_irq_mask = 0x00000000,
	},
};

#if defined(CONFIG_AMCC_PPC440SPE_ADMA)

static u64 ppc440spe_adma_dmamask = DMA_32BIT_MASK;

/* DMA and XOR platform devices' resources */
static struct resource ppc440spe_dma_0_resources[] = {
	{
		.flags = IORESOURCE_MEM,
	},
	{
		.start = DMA0_CS_FIFO_NEED_SERVICE,
		.end = DMA0_CS_FIFO_NEED_SERVICE,
		.flags = IORESOURCE_IRQ
	}
};

static struct resource ppc440spe_dma_1_resources[] = {
	{
		.flags = IORESOURCE_MEM,
	},
	{
		.start = DMA1_CS_FIFO_NEED_SERVICE,
		.end = DMA1_CS_FIFO_NEED_SERVICE,
		.flags = IORESOURCE_IRQ
	}
};

static struct resource ppc440spe_xor_resources[] = {
	{
		.flags = IORESOURCE_MEM,
	},
	{
		.start = XOR_INTERRUPT,
		.end = XOR_INTERRUPT,
		.flags = IORESOURCE_IRQ
	}
};

/* DMA and XOR platform devices' data */
static struct spe_adma_platform_data ppc440spe_dma_0_data = {
	.hw_id  = PPC440SPE_DMA0_ID,
	.capabilities = DMA_CAP_MEMCPY | DMA_CAP_INTERRUPT,
	.pool_size = PAGE_SIZE,
};

static struct spe_adma_platform_data ppc440spe_dma_1_data = {
	.hw_id  = PPC440SPE_DMA1_ID,
	.capabilities =  DMA_CAP_MEMCPY | DMA_CAP_INTERRUPT,
	.pool_size = PAGE_SIZE,
};

static struct spe_adma_platform_data ppc440spe_xor_data = {
	.hw_id  = PPC440SPE_XOR_ID,
	.capabilities = DMA_CAP_XOR | DMA_CAP_INTERRUPT,
	.pool_size = PAGE_SIZE,
};

/* DMA and XOR platform devices definitions */
static struct platform_device ppc440spe_dma_0_channel = {
	.name = "PPC440SPE-ADMA",
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
	.name = "PPC440SPE-ADMA",
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
	.name = "PPC440SPE-ADMA",
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
	i2o_regs_t *i2o_reg;
	dma_regs_t *dma_reg0, *dma_reg1;
	xor_regs_t *xor_reg;
	u32 mask;

	/*
	 * Map registers
	 */
	i2o_reg  = (i2o_regs_t *)ioremap64(I2O_MMAP_BASE, I2O_MMAP_SIZE);
	dma_reg0 = (dma_regs_t *)ioremap64(DMA0_MMAP_BASE, DMA_MMAP_SIZE);
	dma_reg1 = (dma_regs_t *)ioremap64(DMA1_MMAP_BASE, DMA_MMAP_SIZE);
	xor_reg  = (xor_regs_t *)ioremap64(XOR_MMAP_BASE,XOR_MMAP_SIZE);

	/*
	 * Configure h/w
	 */

	/* Reset I2O/DMA */
	mtdcr(DCRN_SDR0_CFGADDR, 0x200);
	mtdcr(DCRN_SDR0_CFGDATA, 0x10000);
	mtdcr(DCRN_SDR0_CFGADDR, 0x200);
	mtdcr(DCRN_SDR0_CFGDATA, 0x0);

	/* Reset XOR */
	out_be32(&xor_reg->crsr, XOR_CRSR_XASR_BIT);
	out_be32(&xor_reg->crrr, XOR_CRSR_64BA_BIT);

	/* Setup the base address of mmaped registers */
	mtdcr(DCRN_I2O0_IBAH, 0x00000004);
	mtdcr(DCRN_I2O0_IBAL, 0x00100001);

	/*  Provide memory regions for DMA's FIFOs: I2O, DMA0 and DMA1 share
	 * the base address of FIFO memory space
	 */
	fifo_buf = kmalloc((DMA0_FIFO_SIZE + DMA1_FIFO_SIZE)<<1, GFP_KERNEL | __GFP_DMA);

	/* SetUp FIFO memory space base address */
	out_le32(&i2o_reg->ifbah, 0);
	out_le32(&i2o_reg->ifbal, ((u32)__pa(fifo_buf)));

	/* zero FIFO size for I2O, DMAs; 0x1000 to enable DMA */
	out_le32(&i2o_reg->ifsiz, 0);
	out_le32(&dma_reg0->fsiz, 0x1000 | ((DMA0_FIFO_SIZE>>3) - 1));
	out_le32(&dma_reg1->fsiz, 0x1000 | ((DMA1_FIFO_SIZE>>3) - 1));

	/* Configure DMA engine */
	out_le32(&dma_reg0->cfg, 0x0D880000);
	out_le32(&dma_reg1->cfg, 0x0D880000);

	/* Clear Status */
	out_le32(&dma_reg0->dsts, ~0);
	out_le32(&dma_reg1->dsts, ~0);

	/* Unmask 'CS FIFO Attention' interrupts */
	mask = in_le32(&i2o_reg->iopim) & ~0x48;
	out_le32(&i2o_reg->iopim, mask);

	/* enable XOR engine interrupt */
	out_be32(&xor_reg->ier, XOR_IE_CBLCI_BIT | XOR_IE_CBCIE_BIT | 0x34000);

	/*
	 * Unmap I2O registers
	 */
	iounmap(i2o_reg);

	/*
	 * Set resource addresses
	 */
	ppc440spe_dma_0_channel.resource[0].start = (resource_size_t)(dma_reg0);
	ppc440spe_dma_0_channel.resource[0].end =
		ppc440spe_dma_0_channel.resource[0].start+DMA_MMAP_SIZE;

	ppc440spe_dma_1_channel.resource[0].start = (resource_size_t)(dma_reg1);
	ppc440spe_dma_1_channel.resource[0].end =
		ppc440spe_dma_1_channel.resource[0].start+DMA_MMAP_SIZE;

	ppc440spe_xor_channel.resource[0].start = (resource_size_t)(xor_reg);
	ppc440spe_xor_channel.resource[0].end =
		ppc440spe_xor_channel.resource[0].start+XOR_MMAP_SIZE;
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
#endif	/* CONFIG_AMCC_PPC440SPE_ADMA */
