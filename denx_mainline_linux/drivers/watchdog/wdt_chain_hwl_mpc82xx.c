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

#include <linux/wdt_chains.h>
#include <asm/cpm2.h>

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

/***********************************************************************
F* Function:     int __init wdt_hwl_init (void) P*A*Z*
 *
P* Parameters:   none
P*
P* Returnvalue:  int
P*                - 0 success
P*                  -ENXIO  The watchdog is not enabled by firmware
 *
Z* Intention:    Initialize the Hardwaredependent functions for the WDT
 *
D* Design:       Haider / wd@denx.de
C* Coding:       Haider / wd@denx.de
V* Verification: wd@denx.de / dzu@denx.de
 ***********************************************************************/
#define MPC82XX_SYPCR_SWE	0x00000004
#define MPC82XX_SYPCR_SWRI	0x00000002
#define MPC82XX_SYPCR_SWP	0x00000001

int __init wdt_hwl_init(void)
{
	/* using MPC82XX internal WDT */
	debugk ("%s: SYPCR=0x%x\n", __FUNCTION__, cpm2_immr->im_siu_conf.siu_82xx.sc_sypcr);
	debugk ("%s: Should be 0x02 (Hardreset) | 0x01 prescaled | 0x04 active\n", __FUNCTION__);
	if ((cpm2_immr->im_siu_conf.siu_82xx.sc_sypcr & MPC82XX_SYPCR_SWE) != MPC82XX_SYPCR_SWE) {
		printk ("WDT not enabled by firmware, SYPCR=0x%x\n",
			cpm2_immr->im_siu_conf.siu_82xx.sc_sypcr);
		return (-ENXIO);
	}

	/* trigger now */
	cpm2_immr->im_siu_conf.siu_82xx.sc_swsr = 0x556C;
	cpm2_immr->im_siu_conf.siu_82xx.sc_swsr = 0xAA39;
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
	cpm2_immr->im_siu_conf.siu_82xx.sc_swsr = 0x556C;
	cpm2_immr->im_siu_conf.siu_82xx.sc_swsr = 0xAA39;
/*	debugk ("%s: WDT serviced\n", __FUNCTION__); */
}
EXPORT_SYMBOL(wdt_hwl_reset);

