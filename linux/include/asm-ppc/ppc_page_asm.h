/*
 * include/asm/ppc_page_asm.h
 *
 * 2007 (C) DENX Software Engineering.
 *
 *  This file is licensed under the terms of the GNU General Public License
 * version 2.  This program is licensed "as is" without any warranty of
 * any kind, whether express or implied.
 *
 *  The page definitions used in the asm files ppc_44x.S and misc.S.
 * PAGE_SIZE = 4K and 64K are only supported on the PPC44x.
 *
 */
#ifndef PPC_PAGE_ASM_H
#define PPC_PAGE_ASM_H

#include <asm/page.h>

#if (PAGE_SHIFT == 12)
/*
 * PAGE_SIZE  4K
 * PAGE_SHIFT 12
 * PTE_SHIFT   9
 * PMD_SHIFT  21
 */
#define PPC44x_TLB_SIZE		PPC44x_TLB_4K
#define PPC44x_PGD_OFF_SH	13 /*(32 - PMD_SHIFT + 2)*/
#define PPC44x_PGD_OFF_M1	19 /*(PMD_SHIFT - 2)*/
#define PPC44x_PTE_ADD_SH	23 /*32 - PMD_SHIFT + PTE_SHIFT + 3*/
#define PPC44x_PTE_ADD_M1	20 /*32 - 3 - PTE_SHIFT*/
#define PPC44x_RPN_M2		19 /*31 - PAGE_SHIFT*/
#elif (PAGE_SHIFT == 14)
/*
 * PAGE_SIZE  16K
 * PAGE_SHIFT 14
 * PTE_SHIFT  7
 * PMD_SHIFT  21
 */
#define PPC44x_TLB_SIZE		PPC44x_TLB_16K
#define PPC44x_PGD_OFF_SH	13 /*(32 - PMD_SHIFT + 2)*/
#define PPC44x_PGD_OFF_M1	19 /*(PMD_SHIFT - 2)*/
#define PPC44x_PTE_ADD_SH	21 /*32 - PMD_SHIFT + PTE_SHIFT + 3*/
#define PPC44x_PTE_ADD_M1	22 /*32 - 3 - PTE_SHIFT*/
#define PPC44x_RPN_M2		17 /*31 - PAGE_SHIFT*/
#elif (PAGE_SHIFT == 16)
/*
 * PAGE_SIZE  64K
 * PAGE_SHIFT 16
 * PTE_SHIFT   5
 * PMD_SHIFT  21
 */
#define PPC44x_TLB_SIZE		PPC44x_TLB_64K
#define PPC44x_PGD_OFF_SH	13 /*(32 - PMD_SHIFT + 2)*/
#define PPC44x_PGD_OFF_M1	19 /*(PMD_SHIFT - 2)*/
#define PPC44x_PTE_ADD_SH	19 /*32 - PMD_SHIFT + PTE_SHIFT + 3*/
#define PPC44x_PTE_ADD_M1	24 /*32 - 3 - PTE_SHIFT*/
#define PPC44x_RPN_M2		15 /*31 - PAGE_SHIFT*/
#else
#error "Unsupported PAGE_SIZE"
#endif

#endif
