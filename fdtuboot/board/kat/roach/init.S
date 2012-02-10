/*
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <ppc_asm.tmpl>
#include <config.h>

/* General */
#define TLB_VALID   0x00000200
#define _256M       0x10000000

/* Supported page sizes */

#define SZ_1K	    0x00000000
#define SZ_4K	    0x00000010
#define SZ_16K	    0x00000020
#define SZ_64K	    0x00000030
#define SZ_256K	    0x00000040
#define SZ_1M	    0x00000050
#define SZ_8M       0x00000060
#define SZ_16M	    0x00000070
#define SZ_256M	    0x00000090

/* Storage attributes */
#define SA_W	    0x00000800	    /* Write-through */
#define SA_I	    0x00000400	    /* Caching inhibited */
#define SA_M	    0x00000200	    /* Memory coherence */
#define SA_G	    0x00000100	    /* Guarded */
#define SA_E	    0x00000080	    /* Endian */

/* Access control */
#define AC_X	    0x00000024	    /* Execute */
#define AC_W	    0x00000012	    /* Write */
#define AC_R	    0x00000009	    /* Read */

/* Some handy macros */

#define EPN(e)		((e) & 0xfffffc00)
#define TLB0(epn,sz)	( (EPN((epn)) | (sz) | TLB_VALID ) )
#define TLB1(rpn,erpn)	( ((rpn)&0xfffffc00) | (erpn) )
#define TLB2(a)		( (a)&0x00000fbf )

#define tlbtab_start\
	mflr    r1  ;\
	bl 0f	    ;

#define tlbtab_end\
	.long 0, 0, 0	;   \
0:	mflr    r0	;   \
	mtlr    r1	;   \
	blr		;

#define tlbentry(epn,sz,rpn,erpn,attr)\
	.long TLB0(epn,sz),TLB1(rpn,erpn),TLB2(attr)


/**************************************************************************
 * TLB TABLE
 *
 * This table is used by the cpu boot code to setup the initial tlb
 * entries. Rather than make broad assumptions in the cpu source tree,
 * this table lets each board set things up however they like.
 *
 *  Pointer to the table is returned in r1
 *
 *************************************************************************/
    .section .bootpg,"ax"
    .globl tlbtab

tlbtab:
	tlbtab_start

	/*
	 * BOOT_CS (FLASH) must be first. Before relocation SA_I can be off to use the
	 * speed up boot process. It is patched after relocation to enable SA_I
	 */
#if 0
	tlbentry( CONFIG_SYS_BOOT_BASE_ADDR, SZ_256M, CONFIG_SYS_BOOT_BASE_ADDR, 1, AC_R|AC_W|AC_X|SA_G|SA_I )
#endif
	tlbentry( CONFIG_SYS_BOOT_BASE_ADDR, SZ_256M, CONFIG_SYS_BOOT_BASE_ADDR, 1, AC_R|AC_W|AC_X|SA_G)

	/* TLB-entry for DDR SDRAM (max 2G) */
	tlbentry( CONFIG_SYS_SDRAM_BASE,  SZ_256M, CONFIG_SYS_SDRAM_BASE,  0, AC_R|AC_W|AC_X|SA_G|SA_I )

#ifdef CONFIG_SYS_SDRAM_BASE1
	tlbentry( CONFIG_SYS_SDRAM_BASE1, SZ_256M, CONFIG_SYS_SDRAM_BASE1, 0, AC_R|AC_W|AC_X|SA_G|SA_I )
#endif
#ifdef CONFIG_SYS_SDRAM_BASE2
	tlbentry( CONFIG_SYS_SDRAM_BASE2, SZ_256M, CONFIG_SYS_SDRAM_BASE2, 0, AC_R|AC_W|AC_X|SA_G|SA_I )
#endif
#ifdef CONFIG_SYS_SDRAM_BASE3
	tlbentry( CONFIG_SYS_SDRAM_BASE3, SZ_256M, CONFIG_SYS_SDRAM_BASE3, 0, AC_R|AC_W|AC_X|SA_G|SA_I )
#endif
#ifdef CONFIG_SYS_SDRAM_BASE4
	tlbentry( CONFIG_SYS_SDRAM_BASE4, SZ_256M, CONFIG_SYS_SDRAM_BASE4, 0, AC_R|AC_W|AC_X|SA_G|SA_I )
#endif
#ifdef CONFIG_SYS_SDRAM_BASE5
	tlbentry( CONFIG_SYS_SDRAM_BASE5, SZ_256M, CONFIG_SYS_SDRAM_BASE5, 0, AC_R|AC_W|AC_X|SA_G|SA_I )
#endif
#ifdef CONFIG_SYS_SDRAM_BASE6
	tlbentry( CONFIG_SYS_SDRAM_BASE6, SZ_256M, CONFIG_SYS_SDRAM_BASE6, 0, AC_R|AC_W|AC_X|SA_G|SA_I )
#endif
#ifdef CONFIG_SYS_SDRAM_BASE7
	tlbentry( CONFIG_SYS_SDRAM_BASE7, SZ_256M, CONFIG_SYS_SDRAM_BASE7, 0, AC_R|AC_W|AC_X|SA_G|SA_I )
#endif

#ifdef CONFIG_SYS_CPLD_BASE
	/* TLB-entry for cpld on EBC */
	tlbentry( CONFIG_SYS_CPLD_BASE, SZ_1M, CONFIG_SYS_CPLD_BASE, 1, AC_R|AC_W|AC_X|SA_G|SA_I )
#endif

#ifdef CONFIG_SYS_SMAP_BASE
	/* TLB-entry for selectmap on EBC */
	tlbentry( CONFIG_SYS_SMAP_BASE, SZ_1M, CONFIG_SYS_SMAP_BASE, 1, AC_R|AC_W|AC_X|SA_G|SA_I )
#endif

#ifdef CONFIG_SYS_FPGA_BASE
	/* TLB-entry for FPGA */
	tlbentry( CONFIG_SYS_FPGA_BASE, SZ_256M, CONFIG_SYS_FPGA_BASE, 1, AC_R|AC_W|AC_X|SA_G|SA_I )
#endif

	/* TLB-entry for Internal Registers & OCM */
	tlbentry( 0xe0000000, SZ_16M, 0xe0000000, 0,  AC_R|AC_W|AC_X|SA_I )

/* use OCM instead instead of risking overlapping memory regions */
#if 0
	/* TLB-entry for init-ram in dcache (SA_I must be turned off!) */
	tlbentry( CONFIG_SYS_INIT_RAM_ADDR, SZ_64K, CONFIG_SYS_INIT_RAM_ADDR, 0, AC_R|AC_W|AC_X|SA_G )
#endif

#if 0
	/*TLB-entry PCI registers*/
	tlbentry( 0xeec00000, SZ_1K, 0xeec00000, 1,  AC_R|AC_W|AC_X|SA_G|SA_I )
#endif

	/* TLB-entry for peripherals */
	tlbentry( 0xef000000, SZ_16M, 0xef000000, 1, AC_R|AC_W|AC_X|SA_G|SA_I)

	tlbtab_end
