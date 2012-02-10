/*
 * arch/ppc/platforms/4xx/roach.c
 *
 * Sequoia board specific routines
 *
 * Copyright 2006-2007 DENX Software Engineering, Stefan Roese <sr@denx.de>
 *
 * Based on bamboo.c from Wade Farnsworth <wfarnsworth@mvista.com>
 *	Copyright 2004 MontaVista Software Inc.
 *	Copyright 2006 AMCC
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/stddef.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/initrd.h>
#include <linux/irq.h>
#include <linux/root_dev.h>
#include <linux/tty.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/ndfc.h>
#include <linux/mtd/physmap.h>

#include <asm/machdep.h>
#include <asm/ocp.h>
#include <asm/bootinfo.h>
#include <asm/ppc4xx_pic.h>
#include <asm/ppcboot.h>

#include <syslib/gen550.h>
#include <syslib/ibm440gx_common.h>

#define BOARDNAME  "440EPx Roach"

extern bd_t __res;

static struct ibm44x_clocks clocks __initdata;

/*
 * Sequoia external IRQ triggering/polarity settings
 */
unsigned char ppc4xx_uic_ext_irq_cfg[] __initdata = {
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE), /* Index0 - IRQ4: */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE), /* Index1 - IRQ7: */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE), /* Index2 - IRQ8: */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE), /* Index3 - IRQ9: */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE), /* Index4 - IRQ0: Ethernet 0 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE), /* Index5 - IRQ1: Ethernet 1 */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE), /* Index6 - IRQ5: */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE), /* Index7 - IRQ6: */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE), /* Index8 - IRQ2: PCI slots */
	(IRQ_SENSE_LEVEL | IRQ_POLARITY_NEGATIVE), /* Index9 - IRQ3: STTM alert */
};

/*
 * NOR FLASH configuration (using mtd physmap driver)
 */

/* start will be added dynamically, end is always fixed */
static struct resource roach_nor_resource = {
		.end   = 0xffffffff,
		.flags = IORESOURCE_MEM,
};

#define RW_PART0_OF	0
#define RW_PART0_SZ	0x180000
#define RW_PART1_SZ	0x280000
/* Partition 2 will be autosized dynamically... */
#define RW_PART3_SZ	0x40000
#define RW_PART4_SZ	0x60000

static struct mtd_partition roach_nor_parts[] = {
	{
		.name = "kernel",
		.offset = 0,
		.size = RW_PART0_SZ
	},
	{
		.name = "root",
		.offset = MTDPART_OFS_APPEND,
		.size = RW_PART1_SZ,
	},
	{
		.name = "user",
		.offset = MTDPART_OFS_APPEND,
/*		.size = RW_PART2_SZ */ /* will be adjusted dynamically */
	},
	{
		.name = "env",
		.offset = MTDPART_OFS_APPEND,
		.size = RW_PART3_SZ,
	},
	{
		.name = "u-boot",
		.offset = MTDPART_OFS_APPEND,
		.size = RW_PART4_SZ,
	}
};

static struct physmap_flash_data roach_nor_data = {
	.width		= 2,
	.parts		= roach_nor_parts,
	.nr_parts	= ARRAY_SIZE(roach_nor_parts),
};

static struct platform_device roach_nor_device = {
	.name		= "physmap-flash",
	.id		= 0,
	.dev = {
			.platform_data = &roach_nor_data,
		},
	.num_resources	= 1,
	.resource	= &roach_nor_resource,
};

#if 0
/*
 * NAND FLASH configuration (using 440EP(x) NDFC)
 */
static struct resource roach_ndfc = {
	.start = (u32)SEQUOIA_NAND_FLASH_REG_ADDR,
	.end = (u32)SEQUOIA_NAND_FLASH_REG_ADDR + SEQUOIA_NAND_FLASH_REG_SIZE,
	.flags = IORESOURCE_MEM,
};

/* todo: add logic to detect booting from NAND (NAND on CS0) */
#define CS_NAND_0	3	/* use chip select 3 for NAND device 0 */

static struct mtd_partition roach_nand_parts[] = {
        {
                .name   = "content",
                .offset = 0,
                .size   = MTDPART_SIZ_FULL,
        }
};

struct ndfc_controller_settings roach_ndfc_settings = {
	.ccr_settings = (NDFC_CCR_BS(CS_NAND_0) |
			 NDFC_CCR_ARAC1),
	.ndfc_erpn = (SEQUOIA_NAND_FLASH_REG_ADDR) >> 32,
};

struct platform_nand_ctrl roach_nand_ctrl = {
	.priv = &roach_ndfc_settings,
};

static struct platform_device roach_ndfc_device = {
	.name = "ndfc-nand",
	.id = 0,
	.dev = {
		.platform_data = &roach_nand_ctrl,
	},
	.num_resources = 1,
	.resource = &roach_ndfc,
};

static struct ndfc_chip_settings roach_chip0_settings = {
	.bank_settings = 0x80002222,
};

static struct nand_ecclayout nand_oob_16 = {
	.eccbytes = 6,
	.eccpos = {9, 10, 11, 13, 14, 15},
	.oobfree = {
		 {.offset = 8,
		  . length = 8}}
};

static struct platform_nand_chip roach_nand_chip0 = {
	.nr_chips = 1,
	.chip_offset = CS_NAND_0,
	.nr_partitions = ARRAY_SIZE(roach_nand_parts),
	.partitions = roach_nand_parts,
	.chip_delay = 50,
	.ecclayout = &nand_oob_16,
	.priv = &roach_chip0_settings,
};

static struct platform_device roach_nand_device = {
	.name = "ndfc-chip",
	.id = 0,
	.num_resources = 1,
	.resource = &roach_ndfc,
	.dev = {
		.platform_data = &roach_nand_chip0,
		.parent = &roach_ndfc_device.dev,
	}
};
#endif

static int roach_setup_flash(void)
{
	roach_nor_resource.start = __res.bi_flashstart;

	/*
	 * Adjust partition 2 to flash size
	 */
	roach_nor_parts[2].size = __res.bi_flashsize -
		RW_PART0_SZ - RW_PART1_SZ - RW_PART3_SZ - RW_PART4_SZ;

	platform_device_register(&roach_nor_device);

	/* todo: add logic to detect booting from NAND (NAND on CS0) */

#if 0
	platform_device_register(&roach_ndfc_device);
	platform_device_register(&roach_nand_device);
#endif

	return 0;
}
arch_initcall(roach_setup_flash);

/*
 * get size of system memory from Board Info .
 */
unsigned long __init roach_find_end_of_memory(void)
{
	/* board info structure defined in /include/asm-ppc/ppcboot.h */
	return  __res.bi_memsize;
}

static void __init roach_calibrate_decr(void)
{
	unsigned int freq;

	if (mfspr(SPRN_CCR1) & CCR1_TCS)
		freq = SEQUOIA_TMRCLK;
	else
		freq = clocks.cpu;

	ibm44x_calibrate_decr(freq);

}

static int roach_show_cpuinfo(struct seq_file *m)
{
	seq_printf(m, "vendor\t\t: AMCC\n");
	seq_printf(m, "machine\t\t: PPC" BOARDNAME "\n");

	return 0;
}

static inline int
roach_map_irq(struct pci_dev *dev, unsigned char idsel, unsigned char pin)
{
	static char pci_irq_table[][4] =
	/*
	 *	PCI IDSEL/INTPIN->INTLINE
	 * 	   A   B   C   D
	 */
	{
		{ 67, 67, 67, 67 },	/* IDSEL 10 - PCI Slot 1, ext. IRQ 2 */
		{ 67, 67, 67, 67 },	/* IDSEL 11 - PCI Slot x, ext. IRQ 2 */
		{ 67, 67, 67, 67 },	/* IDSEL 12 - PCI Slot 0, ext. IRQ 2 */
	};

	const long min_idsel = 10, max_idsel = 12, irqs_per_slot = 4;
	return PCI_IRQ_TABLE_LOOKUP;
}

static void __init roach_set_emacdata(void)
{
	struct ocp_def *def;
	struct ocp_func_emac_data *emacdata;

	/* Set mac_addr, phy mode and unsupported phy features for each EMAC */

	def = ocp_get_one_device(OCP_VENDOR_IBM, OCP_FUNC_EMAC, 0);
	emacdata = def->additions;
	memcpy(emacdata->mac_addr, __res.bi_enetaddr, 6);
	emacdata->phy_mode = PHY_MODE_RGMII;

	def = ocp_get_one_device(OCP_VENDOR_IBM, OCP_FUNC_EMAC, 1);
	emacdata = def->additions;
	memcpy(emacdata->mac_addr, __res.bi_enet1addr, 6);
	emacdata->phy_mode = PHY_MODE_RGMII;

        /*
         * Clear PLB4A0_ACR[WRP] (Write Pipeline Control)
         * This fix will make the MAL burst disabling patch for the Linux
         * EMAC driver obsolete.
         */
	mtdcr(DCRN_PLB4A0_ACR, mfdcr(DCRN_PLB4A0_ACR) & ~(0x80000000 >> 7));
}

static int roach_exclude_device(unsigned char bus, unsigned char devfn)
{
	return (bus == 0 && devfn == 0);
}

static void __init roach_early_serial_map(void)
{
	struct uart_port port;

	/* Setup ioremapped serial port access */
	memset(&port, 0, sizeof(port));
	port.membase = ioremap64(PPC440EPX_UART0_ADDR, 8);
	port.irq = UART0_INT;
	port.uartclk = clocks.uart0;
	port.regshift = 0;
	port.iotype = SERIAL_IO_MEM;
	port.flags = ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST;
	port.line = 0;

	if (early_serial_setup(&port) != 0)
		printk("Early serial init of port 0 failed\n");

#if defined(CONFIG_SERIAL_TEXT_DEBUG) || defined(CONFIG_KGDB)
	/* Configure debug serial access */
	gen550_init(0, &port);
#endif

	port.membase = ioremap64(PPC440EPX_UART1_ADDR, 8);
	port.irq = UART1_INT;
	port.uartclk = clocks.uart1;
	port.line = 1;

	if (early_serial_setup(&port) != 0)
		printk("Early serial init of port 1 failed\n");

#if defined(CONFIG_SERIAL_TEXT_DEBUG) || defined(CONFIG_KGDB)
	/* Configure debug serial access */
	gen550_init(1, &port);
#endif

	port.membase = ioremap64(PPC440EPX_UART2_ADDR, 8);
	port.irq = UART2_INT;
	port.uartclk = clocks.uart2;
	port.line = 2;

	if (early_serial_setup(&port) != 0)
		printk("Early serial init of port 2 failed\n");

#if defined(CONFIG_SERIAL_TEXT_DEBUG) || defined(CONFIG_KGDB)
	/* Configure debug serial access */
	gen550_init(2, &port);
#endif

	port.membase = ioremap64(PPC440EPX_UART3_ADDR, 8);
	port.irq = UART3_INT;
	port.uartclk = clocks.uart3;
	port.line = 3;

	if (early_serial_setup(&port) != 0)
		printk("Early serial init of port 3 failed\n");
}

static void __init roach_setup_arch(void)
{
	roach_set_emacdata();

	/* parm1 = sys clock is OK , parm 2 ser_clock to be checked */
	ibm440gx_get_clocks(&clocks, 33000000, 6 * 1843200);
	ocp_sys_info.opb_bus_freq = clocks.opb;
	ocp_sys_info.plb_bus_freq = clocks.plb;

	/* init to some ~sane value until calibrate_delay() runs */
        loops_per_jiffy = 50000000/HZ;

#ifdef CONFIG_BLK_DEV_INITRD
	if (initrd_start)
		ROOT_DEV = Root_RAM0;
	else {
#ifdef CONFIG_ROOT_NFS
		ROOT_DEV = Root_NFS;
#else
		ROOT_DEV = Root_HDA1;
#endif
	}
#endif

	roach_early_serial_map();

	/* Identify the system */
	printk("AMCC PowerPC " BOARDNAME " Platform\n");
}

static void __init roach_init_irq(void)
{
	ppc4xx_pic_init();
}

void __init platform_init(unsigned long r3, unsigned long r4,
			  unsigned long r5, unsigned long r6, unsigned long r7)
{
	ibm44x_platform_init(r3, r4, r5, r6, r7);

	ppc_md.setup_arch = roach_setup_arch;
	ppc_md.show_cpuinfo = roach_show_cpuinfo;
	ppc_md.find_end_of_memory = roach_find_end_of_memory;
	ppc_md.get_irq = NULL;		/* Set in ppc4xx_pic_init() */

	ppc_md.calibrate_decr = roach_calibrate_decr;
	ppc_md.time_init = NULL;
	ppc_md.set_rtc_time = NULL;
	ppc_md.get_rtc_time = NULL;

	ppc_md.init_IRQ = roach_init_irq;

#ifdef CONFIG_KGDB
	ppc_md.early_serial_map = roach_early_serial_map;
#endif
}
