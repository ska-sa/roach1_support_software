/*
 * Katmai board definitions
 *
 * Copyright 2007 DENX Software Engineering, Stefan Roese <sr@denx.de>
 *
 * Based on yucca.h by Roland Dreier
 *
 * Copyright 2004-2005 MontaVista Software Inc.
 * Copyright (c) 2005 Cisco Systems.  All rights reserved.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#ifdef __KERNEL__
#ifndef __ASM_KATMAI_H__
#define __ASM_KATMAI_H__

#include <platforms/4xx/ppc440spe.h>

#define KATMAI_FLASH_ADDR		0x00000004ff000000ULL
#define KATMAI_FLASH_END		0x00000004ffffffffULL

/* F/W TLB mapping used in bootloader glue to reset EMAC */
#define PPC44x_EMAC0_MR0	0xa0000800

/* Location of MAC addresses in PIBS image */
#define PIBS_FLASH_BASE		0xffe00000
#define PIBS_MAC_BASE		(PIBS_FLASH_BASE+0x1b0400)

/* External timer clock frequency */
#define KATMAI_TMR_CLK		25000000

#define GPIO_VAL(gpio)		(0x80000000 >> (gpio))
#define CFG_GPIO_PCIE_PRESENT0	17
#define CFG_GPIO_PCIE_PRESENT1	21
#define CFG_GPIO_PCIE_PRESENT2	23

/*
 * Serial port defines
 */
#define RS_TABLE_SIZE	3

/* PIBS defined UART mappings, used before early_serial_setup */
#define UART0_IO_BASE	0xa0000200
#define UART1_IO_BASE	0xa0000300
#define UART2_IO_BASE	0xa0000600

#define BASE_BAUD	11059200
#define STD_UART_OP(num)					\
	{ 0, BASE_BAUD, 0, UART##num##_INT,			\
		(ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST),	\
		iomem_base: (void*)UART##num##_IO_BASE,		\
		io_type: SERIAL_IO_MEM},

#define SERIAL_PORT_DFNS	\
	STD_UART_OP(0)		\
	STD_UART_OP(1)		\
	STD_UART_OP(2)

/* PCI support */
#define KATMAI_PCIX_LOWER_IO	0x00000000
#define KATMAI_PCIX_UPPER_IO	0x0000ffff
#define KATMAI_PCIX_LOWER_MEM	0x80000000
#define KATMAI_PCIX_UPPER_MEM	0x8fffffff
#define KATMAI_PCIX_MEM_SIZE	0x10000000
#define KATMAI_PCIX_MEM_OFFSET	0x00000000

#define KATMAI_PCIE_LOWER_MEM	0x90000000
#define KATMAI_PCIE_MEM_SIZE	0x10000000
#define BOARD_PCIE_MEM_SIZE	KATMAI_PCIE_MEM_SIZE	/* used in syslib/ppc440spe_pcie.c */
#define KATMAI_PCIE_MEM_OFFSET	0x00000000

#endif				/* __ASM_KATMAI_H__ */
#endif				/* __KERNEL__ */
