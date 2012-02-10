/*
 * muas3001 board support
 *
 * Copyright 2008 DENX Software Engineering GmbH
 * Author: Heiko Schocher <hs@denx.de>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/fsl_devices.h>
#include <linux/of_platform.h>
#include <linux/io.h>

#include <asm/cpm2.h>
#include <asm/udbg.h>
#include <asm/machdep.h>
#include <asm/time.h>

#include <platforms/82xx/pq2.h>

#include <sysdev/fsl_soc.h>
#include <sysdev/cpm2_pic.h>

#include "pq2ads.h"
#include "pq2.h"

static void __init muas3001_pic_init(void)
{
	struct device_node *np = of_find_compatible_node(NULL, NULL,
	                                                 "fsl,cpm2-pic");
	if (!np) {
		printk(KERN_ERR "PIC init: can not find fsl,cpm2-pic node\n");
		return;
	}

	cpm2_pic_init(np);
	of_node_put(np);

}

struct cpm_pin {
	int port, pin, flags;
};

static struct cpm_pin muas3001_pins[] = {
	/* SMC1 */
	{3,  9, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},	/* TxD */
	{3,  8, CPM_PIN_INPUT | CPM_PIN_PRIMARY},	/* RxD */

	/* SMC2 */
	{2, 15, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},	/* TxD */
	{0,  8, CPM_PIN_INPUT | CPM_PIN_PRIMARY},	/* RxD */

	/* SCC1 */
	{1, 28, CPM_PIN_OUTPUT | CPM_PIN_SECONDARY},	/* Txd */
	{3, 31, CPM_PIN_INPUT | CPM_PIN_PRIMARY},	/* RxD */

	/* SCC2 */
	{1, 12, CPM_PIN_OUTPUT | CPM_PIN_SECONDARY},	/* TxD */
	{1, 15, CPM_PIN_INPUT | CPM_PIN_PRIMARY},	/* RxD */

	/* SCC3 */
	{1,  9, CPM_PIN_OUTPUT | CPM_PIN_SECONDARY},	/* TxD */
	{1, 14, CPM_PIN_INPUT | CPM_PIN_PRIMARY},	/* RxD */

	/* SCC4 */
	{3, 21, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},	/* TxD */
	{3, 22, CPM_PIN_INPUT | CPM_PIN_PRIMARY},	/* RxD */

	/* FCC1 */
	{0, 14, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{0, 15, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{0, 16, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{0, 17, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{0, 18, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{0, 19, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{0, 20, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{0, 21, CPM_PIN_OUTPUT | CPM_PIN_PRIMARY},
	{0, 26, CPM_PIN_INPUT | CPM_PIN_SECONDARY},
	{0, 27, CPM_PIN_INPUT | CPM_PIN_SECONDARY},
	{0, 28, CPM_PIN_OUTPUT | CPM_PIN_SECONDARY},
	{0, 29, CPM_PIN_OUTPUT | CPM_PIN_SECONDARY},
	{0, 30, CPM_PIN_INPUT | CPM_PIN_SECONDARY},
	{0, 31, CPM_PIN_INPUT | CPM_PIN_SECONDARY},
	{2, 21, CPM_PIN_INPUT | CPM_PIN_PRIMARY},
	{2, 22, CPM_PIN_INPUT | CPM_PIN_PRIMARY},

};

static void __init init_ioports(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(muas3001_pins); i++) {
		struct cpm_pin *pin = &muas3001_pins[i];
		cpm2_set_pin(pin->port, pin->pin, pin->flags);
	}

	cpm2_smc_clk_setup(CPM_CLK_SMC1, CPM_BRG7);
	cpm2_smc_clk_setup(CPM_CLK_SMC2, CPM_BRG8);
	cpm2_clk_setup(CPM_CLK_SCC1, CPM_BRG1, CPM_CLK_RX);
	cpm2_clk_setup(CPM_CLK_SCC1, CPM_BRG1, CPM_CLK_TX);
	cpm2_clk_setup(CPM_CLK_SCC2, CPM_BRG2, CPM_CLK_RX);
	cpm2_clk_setup(CPM_CLK_SCC2, CPM_BRG2, CPM_CLK_TX);
	cpm2_clk_setup(CPM_CLK_SCC3, CPM_BRG3, CPM_CLK_RX);
	cpm2_clk_setup(CPM_CLK_SCC3, CPM_BRG3, CPM_CLK_TX);
	cpm2_clk_setup(CPM_CLK_SCC4, CPM_BRG4, CPM_CLK_RX);
	cpm2_clk_setup(CPM_CLK_SCC4, CPM_BRG4, CPM_CLK_TX);
	cpm2_clk_setup(CPM_CLK_FCC1, CPM_CLK11, CPM_CLK_RX);
	cpm2_clk_setup(CPM_CLK_FCC1, CPM_CLK12, CPM_CLK_TX);
}

#define MPC82XX_BCR_PLDP 0x00800000 /* Pipeline Maximum Depth */
static void __init muas3001_setup_arch(void)
{
	if (ppc_md.progress)
		ppc_md.progress("muas3001_setup_arch()", 0);

	cpm2_reset();

	/* When this is set, snooping CPM DMA from RAM causes
	 * machine checks.  See erratum SIU18.
	 */
	clrbits32(&cpm2_immr->im_siu_conf.siu_82xx.sc_bcr, MPC82XX_BCR_PLDP);
	init_ioports();

	if (ppc_md.progress)
		ppc_md.progress("muas3001_setup_arch(), finish", 0);
}

static struct of_device_id __initdata of_bus_ids[] = {
	{ .name = "soc", },
	{ .name = "cpm", },
	{ .name = "localbus", },
	{},
};

static int __init declare_of_platform_devices(void)
{
	/* Publish the QE devices */
	of_platform_bus_probe(NULL, of_bus_ids, NULL);
	return 0;
}
machine_device_initcall(muas3001, declare_of_platform_devices);

/*
 * Called very early, device-tree isn't unflattened
 */
static int __init muas3001_probe(void)
{
	unsigned long root = of_get_flat_dt_root();
	return of_flat_dt_is_compatible(root, "iai,muas3001");
}

define_machine(muas3001)
{
	.name = "IAI muas3001",
	.probe = muas3001_probe,
	.setup_arch = muas3001_setup_arch,
	.init_IRQ = muas3001_pic_init,
	.get_irq = cpm2_get_irq,
	.calibrate_decr = generic_calibrate_decr,
	.restart = pq2_restart,
	.progress = udbg_progress,
};
