/*
 * (C) Copyright Heiko Schocher <hs@denx.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>			/* for character devices	*/
#include <linux/version.h>
#include <linux/init.h>			/* for __initfunc		*/
#include <linux/slab.h>			/* for kmalloc and friends      */

#include <linux/wdt_chains.h>
#include <asm/8xx_immap.h>
#include <asm/mpc8xx.h>

#ifdef CONFIG_8xx
static volatile immap_t *immr = NULL;	/* pointer to register-structure	*/
#endif

#ifdef DEBUG
# define debugk(fmt,args...) printk(fmt ,##args)
#else
# define debugk(fmt,args...)
#endif


int wdt_chain_hwl_start(void)
{
	return 0;
}

int wdt_chain_hwl_stop(void)
{
	return 0;
}

#define IMAP_ADDR_CHAIN 0xf0000000
/***********************************************************************
F* Function:     int __init wdt_hwl_init (void) P*A*Z*
 *
P* Parameters:   none
P*
P* Returnvalue:  int
P*                - 0 success
P*                  -ENXIO  (LWMON) The watchdog is not enabled by firmware
 *
Z* Intention:    Initialize the Hardwaredependent functions for the WDT
 *
D* Design:       Haider / wd@denx.de
C* Coding:       Haider / wd@denx.de
V* Verification: wd@denx.de / dzu@denx.de
 ***********************************************************************/
int __init wdt_hwl_init(void)
{
	/* get pointer for SYPCR and SWSR */
	if (!immr) {
		immr = (immap_t *) IMAP_ADDR_CHAIN;
	}

	debugk("%s: SYPCR=0x%x\n", __FUNCTION__, immr->im_siu_conf.sc_sypcr);

#ifndef	CONFIG_LWMON	/* LWMON uses external MAX708TESA watchdog */
	if ((immr->im_siu_conf.sc_sypcr & 0x00000004) == 0) {
		printk("WDT_8xx: SWT not enabled by firmware, SYPCR=0x%x\n",
			immr->im_siu_conf.sc_sypcr);
		return (-ENXIO);
	}
#endif

#ifdef CONFIG_LWMON	/* LWMON uses external MAX708TESA watchdog */

	immr->im_ioport.iop_padat ^= 0x0100;

#else			/* use MPC8xx internal SWT */
	/* set SYPCR[SWRI]...wdt-timeout causes reset */
	immr->im_siu_conf.sc_sypcr |= (
				0x00000002	/* SWRI - SWT causes HRESET  */
				|
				0x00000001	/* SWP  - activate prescaler */
				|
				0xFFFF0000	/* SWTC - max. timer count   */
				);

	debugk("%s: SYPCR=0x%x\n", __FUNCTION__, immr->im_siu_conf.sc_sypcr);

	/* trigger now */
	immr->im_siu_conf.sc_swsr = 0x556C;
	immr->im_siu_conf.sc_swsr = 0xAA39;
#endif

	return 0;
}

/***********************************************************************
F* Function:	 void wdt_hwl_reset (void) P*A*Z*
 *
P* Parameters:	 none
P*
P* Returnvalue:	 none
 *
Z* Intention:	 Reset the hardware watchdog.
 *
D* Design:	 wd@denx.de
C* Coding:	 wd@denx.de
V* Verification: wd@denx.de / dzu@denx.de
 ***********************************************************************/
void wdt_hwl_reset(void)
{
	volatile immap_t *imap = (immap_t *) IMAP_ADDR_CHAIN;
#if defined(CONFIG_LWMON)
	/*
	 * The LWMON board uses a MAX706TESA Watchdog
	 * with the trigger pin connected to port PA.7
	 *
	 * The port has already been set up in the firmware,
	 * so we just have to toggle it.
	 */
	imap->im_ioport.iop_padat ^= 0x0100;
	/*
	 * Do NOT add a call to "debugk()" here;
	 * it would be called TOO often.
	 */
#else
	/*
	 * All other boards use the MPC8xx Internal Watchdog
	 */
	imap->im_siu_conf.sc_swsr = 0x556C;
	imap->im_siu_conf.sc_swsr = 0xAA39;
	debugk ("%s: WDT serviced\n", __FUNCTION__);
#endif /* CONFIG_LWMON */
}
EXPORT_SYMBOL(wdt_hwl_reset);

