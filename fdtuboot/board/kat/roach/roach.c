/*
 * (C) Copyright 2006
 * Stefan Roese, DENX Software Engineering, sr@denx.de.
 *
 * (C) Copyright 2006
 * Jacqueline Pira-Ferriol, AMCC/IBM, jpira-ferriol@fr.ibm.com
 * Alain Saurel,	    AMCC/IBM, alain.saurel@fr.ibm.com
 *
 * (C) 2008
 * Marc Welz,               KAT
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
 */

/* #define DEBUG */

#include <common.h>
#include <asm/processor.h>
#include <ppc440.h>

#include "roach.h"
#include <roach/roach_bsp.h>
#include <i2c.h>

#define FLASH_BASE 0xFC000000
#define BOARD_SERIAL_OFFSET 0xB8

DECLARE_GLOBAL_DATA_PTR;

extern flash_info_t flash_info[CONFIG_SYS_MAX_FLASH_BANKS]; /* info for FLASH chips	*/

ulong flash_get_size (ulong base, int banknum);

int board_early_init_f(void)
{
	u32 sdr0_cust0;
	u32 sdr0_pfc1, sdr0_pfc2;
	u32 reg;

#if 0
	mtdcr(ebccfga, xbcfg);
	mtdcr(ebccfgd, 0xb8400000);
#endif
  /* same as above */
  mtebc(xbcfg, 0xb8400000);

	/*--------------------------------------------------------------------
	 * Setup the GPIO pins
	 *-------------------------------------------------------------------*/
	/* test-only: take GPIO init from pcs440ep ???? in config file */

	/* These should be symbolic instead of just magic constants */

	out32(GPIO0_OR,    0x00000000);
	out32(GPIO0_TCR,   0x00000000);
	out32(GPIO0_OSRL,  0x55555400);
	out32(GPIO0_OSRH,  0x55005000);
	out32(GPIO0_TSRL,  0x55555500);
	out32(GPIO0_TSRH,  0x55005000);
	out32(GPIO0_ISR1L, 0x55500100);
	out32(GPIO0_ISR1H, 0x00000000);
	out32(GPIO0_ISR2L, 0x00000000);
	out32(GPIO0_ISR2H, 0x00000100);
	out32(GPIO0_ISR3L, 0x00000000);
	out32(GPIO0_ISR3H, 0x00000000);

	out32(GPIO1_OR,    0x00000000);
	out32(GPIO1_TCR,   0xc2000000);
	out32(GPIO1_OSRL,  0x5c280000);
	out32(GPIO1_OSRH,  0x00000000);
	out32(GPIO1_TSRL,  0x0c000000);
	out32(GPIO1_TSRH,  0x00000000);
	out32(GPIO1_ISR1L, 0x00005550);
	out32(GPIO1_ISR1H, 0x00000000);
	out32(GPIO1_ISR2L, 0x00050000);
	out32(GPIO1_ISR2H, 0x00000000);
	out32(GPIO1_ISR3L, 0x01400000);
	out32(GPIO1_ISR3H, 0x00000000);

	/*--------------------------------------------------------------------
	 * Setup the interrupt controller polarities, triggers, etc.
	 *-------------------------------------------------------------------*/
	mtdcr(uic0sr, 0xffffffff);	/* clear all */
	mtdcr(uic0er, 0x00000000);	/* disable all */
	mtdcr(uic0cr, 0x00000005);	/* ATI & UIC1 crit are critical */
	mtdcr(uic0pr, 0xfffff7ff);	/* per ref-board manual */
	mtdcr(uic0tr, 0x00000000);	/* per ref-board manual */
	mtdcr(uic0vr, 0x00000000);	/* int31 highest, base=0x000 */
	mtdcr(uic0sr, 0xffffffff);	/* clear all */

	mtdcr(uic1sr, 0xffffffff);	/* clear all */
	mtdcr(uic1er, 0x00000000);	/* disable all */
	mtdcr(uic1cr, 0x00000000);	/* all non-critical */
	mtdcr(uic1pr, 0xffffffff);	/* per ref-board manual */
	mtdcr(uic1tr, 0x00000000);	/* per ref-board manual */
	mtdcr(uic1vr, 0x00000000);	/* int31 highest, base=0x000 */
	mtdcr(uic1sr, 0xffffffff);	/* clear all */

	mtdcr(uic2sr, 0xffffffff);	/* clear all */
	mtdcr(uic2er, 0x00000000);	/* disable all */
	mtdcr(uic2cr, 0x00000000);	/* all non-critical */
	mtdcr(uic2pr, 0xffffffff);	/* per ref-board manual */
	mtdcr(uic2tr, 0x00000000);	/* per ref-board manual */
	mtdcr(uic2vr, 0x00000000);	/* int31 highest, base=0x000 */
	mtdcr(uic2sr, 0xffffffff);	/* clear all */

	/* select Ethernet pins */
	mfsdr(SDR0_PFC1, sdr0_pfc1);
	sdr0_pfc1 = (sdr0_pfc1 & ~SDR0_PFC1_SELECT_MASK) | SDR0_PFC1_SELECT_CONFIG_4;
	mfsdr(SDR0_PFC2, sdr0_pfc2);
	sdr0_pfc2 = (sdr0_pfc2 & ~SDR0_PFC2_SELECT_MASK) | SDR0_PFC2_SELECT_CONFIG_4;
	mtsdr(SDR0_PFC2, sdr0_pfc2);
	mtsdr(SDR0_PFC1, sdr0_pfc1);

	/* PCI arbiter enabled */
	mfsdr(sdr_pci0, reg);
	mtsdr(sdr_pci0, 0x80000000 | reg);

	/* setup NAND FLASH (not used) */
	mfsdr(SDR0_CUST0, sdr0_cust0);
	sdr0_cust0 = SDR0_CUST0_MUX_NDFC_SEL; /* select but don't enable */
	mtsdr(SDR0_CUST0, sdr0_cust0);

	return 0;
}

#ifdef CONFIG_BOARD_EARLY_INIT_R

/* this was added to track down a caching problem, no need for it now */

static void dump_region(char *prefix)
{
  int i;
  volatile char *ptr;

  ptr = (char *)FLASH_BASE;

  printf("flash: %s", prefix);
  for(i = 0x20; i < 40; i++){
    printf(" 0x%02x", ptr[i]);
  }
  printf("\r\n");
}

static void dump_bus_info()
{
  uint cr, ap, bear, bsr, cfg;

	printf("\r\n");

  mfebc(pb0cr, cr);
  mfebc(pb0ap, ap);
  printf("ebc: cs0 [flash] ap=0x%08x/0x%08x, cr=0x%08x/0x%08x\n", ap, CONFIG_SYS_EBC_PB0AP, cr, CONFIG_SYS_EBC_PB0CR);

  mfebc(pb1cr, cr);
  mfebc(pb1ap, ap);
  printf("ebc: cs1 [fpga]  ap=0x%08x/0x%08x, cr=0x%08x/0x%08x\n", ap, CONFIG_SYS_EBC_PB1AP, cr, CONFIG_SYS_EBC_PB1CR);

  mfebc(pb2cr, cr);
  mfebc(pb2ap, ap);
  printf("ebc: cs2 [cpld]  ap=0x%08x/0x%08x, cr=0x%08x/0x%08x\n", ap, CONFIG_SYS_EBC_PB2AP, cr, CONFIG_SYS_EBC_PB2CR);

  mfebc(pb3cr, cr);
  mfebc(pb3ap, ap);
  printf("ebc: cs3 [smap]  ap=0x%08x, cr=0x%08x\n", ap, cr);

  mfebc(pb4cr, cr);
  mfebc(pb4ap, ap);
  printf("ebc: cs4 []      ap=0x%08x, cr=0x%08x\n", ap, cr);

  mfebc(pb5cr, cr);
  mfebc(pb5ap, ap);
  printf("ebc: cs5 []      ap=0x%08x, cr=0x%08x\n", ap, cr);

  mfebc(pbear, bear);
  mfebc(pbesr, bsr);
  mfebc(xbcfg, cfg);

  printf("ebc: bear=0x%x, bsr=0x%x, cfg=0x%x\n", bear, bsr, cfg);
}

int board_early_init_r (void)
{
	int i,data_val, major, minor;
/*NOTE:Marc i commented out all the debug in this function*/

#if 0
	printf("\r\n");

  dump_bus_info();

	printf("CFI Trial in board_early_init_r\r\n");
	//*((volatile unsigned short *)(FLASH_BASE + 0x000)) = (unsigned short)(0x00FF);/*Reset  Mode*/

	*((volatile unsigned short *)(FLASH_BASE + 0xAA)) = (unsigned short)(0x0098);/*CFI Entry Mode*/

  dump_region("in cfi");

	*((volatile unsigned short *)(FLASH_BASE + 0x55)) = (unsigned short)(0x0098); /* extra cfi */

  dump_region("more cfi");

	*((volatile unsigned short *)(FLASH_BASE)) = (unsigned short)(0x00ff);
	*((volatile unsigned short *)(FLASH_BASE + 0xAA)) = (unsigned short)(0x0098); /* extra cfi */

  dump_region("more cfi");

  dump_bus_info();
#endif

  /*Code inserted for getting serial and revision numbers via i2c*/

  printf("SERIAL NUMBER: ");

  for(i = 0; i < 6; i++){
	  data_val = (monitor_get_val(BOARD_SERIAL_OFFSET + i) & 0xFF);
	  if((i == 0) && !data_val){
			printf("(not set)");
      break;
	  }
	  printf("%c",data_val);
  }

  printf("\n");

  /*Reading revision numbers*/
  major = monitor_get_val(RBSP_SYS_MAJOR / 2);
  if(major == -1){
	  return -1;
  }
  minor = monitor_get_val(RBSP_SYS_MINOR / 2);
  if(minor == -1){
	  return -1;
  }
  printf("REV:%d.%d\n", major, minor);

  return 0;
}
#endif /* CONFIG_BOARD_EARLY_INIT_R */

/*---------------------------------------------------------------------------+
  | misc_init_r.
  +---------------------------------------------------------------------------*/
int misc_init_r(void)
{
	uint pbcr;
	int size_val = 0;
	u32 reg;
	unsigned long usb2d0cr = 0;
	unsigned long usb2phy0cr, usb2h0cr = 0;
	unsigned long sdr0_pfc1;
	unsigned long addr;
	char *act = getenv("usbact");

#if 0
  printf("misc setup from ram\n");
#endif

	/*
	 * FLASH stuff...
	 */

	/* Re-do sizing to get full correct info */

	/* adjust flash start and offset */
	gd->bd->bi_flashstart = 0 - gd->bd->bi_flashsize;
	gd->bd->bi_flashoffset = 0;

  mfebc(pb0cr, pbcr);

#if 0
	mtdcr(ebccfga, pb0cr);
	pbcr = mfdcr(ebccfgd);
#endif

	switch (gd->bd->bi_flashsize) {
	case 1 << 20:
		size_val = 0;
		break;
	case 2 << 20:
		size_val = 1;
		break;
	case 4 << 20:
		size_val = 2;
		break;
	case 8 << 20:
		size_val = 3;
		break;
	case 16 << 20:
		size_val = 4;
		break;
	case 32 << 20:
		size_val = 5;
		break;
	case 64 << 20:
		size_val = 6;
		break;
	case 128 << 20:
		size_val = 7;
		break;
	}

  debug("ebc: sizeval=%d from flashsize=%lu, flashstart=0x%lx\n", size_val, gd->bd->bi_flashsize, gd->bd->bi_flashstart);

	pbcr = (pbcr & 0x0001ffff) | gd->bd->bi_flashstart | (size_val << 17);

  debug("ebc: cs0 [flash] cr=0x%08x\n", pbcr);
  mtebc(pb0cr, pbcr);

  /* WARNING: this seems redundant, as similar is already */
  /* done in cpu/ppc4xx/cpu_init.c */

#ifdef CONFIG_SYS_FPGA_BASE
        debug("ebc: cs1 [fpga]  ap=0x%08x\n", CONFIG_SYS_EBC_PB1AP);
        mtebc(pb1ap, CONFIG_SYS_EBC_PB1AP);
        debug("ebc: cs1 [fpga]  cr=0x%08x\n", CONFIG_SYS_EBC_PB1CR);
        mtebc(pb1cr, CONFIG_SYS_EBC_PB1CR);
#endif

#ifdef CONFIG_SYS_CPLD_BASE
        debug("ebc: cs2 [cpld]  ap=0x%08x\n", CONFIG_SYS_EBC_PB2AP);
        mtebc(pb2ap, CONFIG_SYS_EBC_PB2AP);
        debug("ebc: cs2 [cpld]  cr=0x%08x\n", CONFIG_SYS_EBC_PB2CR);
        mtebc(pb2cr, CONFIG_SYS_EBC_PB2CR);
#endif

#ifdef CONFIG_SYS_SMAP_BASE
        debug("ebc: cs4 [smap]  ap=0x%08x\n", CONFIG_SYS_EBC_PB4AP);
        mtebc(pb4ap, CONFIG_SYS_EBC_PB4AP);
        debug("ebc: cs4 [smap]  cr=0x%08x\n", CONFIG_SYS_EBC_PB4CR);
        mtebc(pb4cr, CONFIG_SYS_EBC_PB4CR);
#endif

#if 0
	mtdcr(ebccfga, pb0cr);
	mtdcr(ebccfgd, pbcr);
#endif

	/*
	 * Re-check to get correct base address
	 */
	flash_get_size(gd->bd->bi_flashstart, 0);

#ifdef CFG_ENV_IS_IN_FLASH
	/* Monitor protection ON by default */
	(void)flash_protect(FLAG_PROTECT_SET,
			    -CFG_MONITOR_LEN,
			    0xffffffff,
			    &flash_info[0]);

	/* Env protection ON by default */
	(void)flash_protect(FLAG_PROTECT_SET,
			    CFG_ENV_ADDR_REDUND,
			    CFG_ENV_ADDR_REDUND + 2*CFG_ENV_SECT_SIZE - 1,
			    &flash_info[0]);
#endif

#if 1
	mfsdr(sdr_amp1, addr);
	mtsdr(sdr_amp1, (addr & 0x000000FF) | 0x0000FF00);

	addr = mfdcr(plb3_acr);
	mtdcr(plb3_acr, addr | 0x80000000);

	mfsdr(sdr_amp0, addr);
	mtsdr(sdr_amp0, (addr & 0x000000FF) | 0x0000FF00);

	addr = mfdcr(plb4_acr) | 0xa0000000;	/* Was 0x8---- */
	mtdcr(plb4_acr, addr);

	/*-------------------------------------------------------------------------+
	  | Set Nebula PLB4 arbiter to fair mode.
	  +-------------------------------------------------------------------------*/
	/* Segment0 */
	addr = (mfdcr(plb0_acr) & ~plb0_acr_ppm_mask) | plb0_acr_ppm_fair;
	addr = (addr & ~plb0_acr_hbu_mask) | plb0_acr_hbu_enabled;
	addr = (addr & ~plb0_acr_rdp_mask) | plb0_acr_rdp_4deep;
	addr = (addr & ~plb0_acr_wrp_mask) | plb0_acr_wrp_2deep;
	mtdcr(plb0_acr, addr);

	/* Segment1 */
	addr = (mfdcr(plb1_acr) & ~plb1_acr_ppm_mask) | plb1_acr_ppm_fair;
	addr = (addr & ~plb1_acr_hbu_mask) | plb1_acr_hbu_enabled;
	addr = (addr & ~plb1_acr_rdp_mask) | plb1_acr_rdp_4deep;
	addr = (addr & ~plb1_acr_wrp_mask) | plb1_acr_wrp_2deep;
	mtdcr(plb1_acr, addr);
#endif

	/*
	 * USB suff...
	 */
	if (act == NULL || strcmp(act, "hostdev") == 0)	{
		/* SDR Setting */
		mfsdr(SDR0_PFC1, sdr0_pfc1);
		mfsdr(SDR0_USB0, usb2d0cr);
		mfsdr(SDR0_USB2PHY0CR, usb2phy0cr);
		mfsdr(SDR0_USB2H0CR, usb2h0cr);

		usb2phy0cr = usb2phy0cr &~SDR0_USB2PHY0CR_XOCLK_MASK;
		usb2phy0cr = usb2phy0cr | SDR0_USB2PHY0CR_XOCLK_EXTERNAL;	/*0*/
		usb2phy0cr = usb2phy0cr &~SDR0_USB2PHY0CR_WDINT_MASK;
		usb2phy0cr = usb2phy0cr | SDR0_USB2PHY0CR_WDINT_16BIT_30MHZ;	/*1*/
		usb2phy0cr = usb2phy0cr &~SDR0_USB2PHY0CR_DVBUS_MASK;
		usb2phy0cr = usb2phy0cr | SDR0_USB2PHY0CR_DVBUS_PURDIS;		/*0*/
		usb2phy0cr = usb2phy0cr &~SDR0_USB2PHY0CR_DWNSTR_MASK;
		usb2phy0cr = usb2phy0cr | SDR0_USB2PHY0CR_DWNSTR_HOST;		/*1*/
		usb2phy0cr = usb2phy0cr &~SDR0_USB2PHY0CR_UTMICN_MASK;
		usb2phy0cr = usb2phy0cr | SDR0_USB2PHY0CR_UTMICN_HOST;		/*1*/

		/* An 8-bit/60MHz interface is the only possible alternative
		   when connecting the Device to the PHY */
		usb2h0cr   = usb2h0cr &~SDR0_USB2H0CR_WDINT_MASK;
		usb2h0cr   = usb2h0cr | SDR0_USB2H0CR_WDINT_16BIT_30MHZ;	/*1*/

		/* To enable the USB 2.0 Device function through the UTMI interface */
		usb2d0cr = usb2d0cr &~SDR0_USB2D0CR_USB2DEV_EBC_SEL_MASK;
		usb2d0cr = usb2d0cr | SDR0_USB2D0CR_USB2DEV_SELECTION;		/*1*/

		sdr0_pfc1 = sdr0_pfc1 &~SDR0_PFC1_UES_MASK;
		sdr0_pfc1 = sdr0_pfc1 | SDR0_PFC1_UES_USB2D_SEL;		/*0*/

		mtsdr(SDR0_PFC1, sdr0_pfc1);
		mtsdr(SDR0_USB0, usb2d0cr);
		mtsdr(SDR0_USB2PHY0CR, usb2phy0cr);
		mtsdr(SDR0_USB2H0CR, usb2h0cr);

		/*clear resets*/
		udelay (1000);
		mtsdr(SDR0_SRST1, 0x00000000);
		udelay (1000);
		mtsdr(SDR0_SRST0, 0x00000000);

		printf("USB:   Host(int phy) Device(ext phy)\n");

	} else if (strcmp(act, "dev") == 0) {
		/*-------------------PATCH-------------------------------*/
		mfsdr(SDR0_USB2PHY0CR, usb2phy0cr);

		usb2phy0cr = usb2phy0cr &~SDR0_USB2PHY0CR_XOCLK_MASK;
		usb2phy0cr = usb2phy0cr | SDR0_USB2PHY0CR_XOCLK_EXTERNAL;	/*0*/
		usb2phy0cr = usb2phy0cr &~SDR0_USB2PHY0CR_DVBUS_MASK;
		usb2phy0cr = usb2phy0cr | SDR0_USB2PHY0CR_DVBUS_PURDIS;		/*0*/
		usb2phy0cr = usb2phy0cr &~SDR0_USB2PHY0CR_DWNSTR_MASK;
		usb2phy0cr = usb2phy0cr | SDR0_USB2PHY0CR_DWNSTR_HOST;		/*1*/
		usb2phy0cr = usb2phy0cr &~SDR0_USB2PHY0CR_UTMICN_MASK;
		usb2phy0cr = usb2phy0cr | SDR0_USB2PHY0CR_UTMICN_HOST;		/*1*/
		mtsdr(SDR0_USB2PHY0CR, usb2phy0cr);

		udelay (1000);
		mtsdr(SDR0_SRST1, 0x672c6000);

		udelay (1000);
		mtsdr(SDR0_SRST0, 0x00000080);

		udelay (1000);
		mtsdr(SDR0_SRST1, 0x60206000);

		*(unsigned int *)(0xe0000350) = 0x00000001;

		udelay (1000);
		mtsdr(SDR0_SRST1, 0x60306000);
		/*-------------------PATCH-------------------------------*/

		/* SDR Setting */
		mfsdr(SDR0_USB2PHY0CR, usb2phy0cr);
		mfsdr(SDR0_USB2H0CR, usb2h0cr);
		mfsdr(SDR0_USB0, usb2d0cr);
		mfsdr(SDR0_PFC1, sdr0_pfc1);

		usb2phy0cr = usb2phy0cr &~SDR0_USB2PHY0CR_XOCLK_MASK;
		usb2phy0cr = usb2phy0cr | SDR0_USB2PHY0CR_XOCLK_EXTERNAL;	/*0*/
		usb2phy0cr = usb2phy0cr &~SDR0_USB2PHY0CR_WDINT_MASK;
		usb2phy0cr = usb2phy0cr | SDR0_USB2PHY0CR_WDINT_8BIT_60MHZ;	/*0*/
		usb2phy0cr = usb2phy0cr &~SDR0_USB2PHY0CR_DVBUS_MASK;
		usb2phy0cr = usb2phy0cr | SDR0_USB2PHY0CR_DVBUS_PUREN;		/*1*/
		usb2phy0cr = usb2phy0cr &~SDR0_USB2PHY0CR_DWNSTR_MASK;
		usb2phy0cr = usb2phy0cr | SDR0_USB2PHY0CR_DWNSTR_DEV;		/*0*/
		usb2phy0cr = usb2phy0cr &~SDR0_USB2PHY0CR_UTMICN_MASK;
		usb2phy0cr = usb2phy0cr | SDR0_USB2PHY0CR_UTMICN_DEV;		/*0*/

		usb2h0cr   = usb2h0cr &~SDR0_USB2H0CR_WDINT_MASK;
		usb2h0cr   = usb2h0cr | SDR0_USB2H0CR_WDINT_8BIT_60MHZ;		/*0*/

		usb2d0cr = usb2d0cr &~SDR0_USB2D0CR_USB2DEV_EBC_SEL_MASK;
		usb2d0cr = usb2d0cr | SDR0_USB2D0CR_EBC_SELECTION;		/*0*/

		sdr0_pfc1 = sdr0_pfc1 &~SDR0_PFC1_UES_MASK;
		sdr0_pfc1 = sdr0_pfc1 | SDR0_PFC1_UES_EBCHR_SEL;		/*1*/

		mtsdr(SDR0_USB2H0CR, usb2h0cr);
		mtsdr(SDR0_USB2PHY0CR, usb2phy0cr);
		mtsdr(SDR0_USB0, usb2d0cr);
		mtsdr(SDR0_PFC1, sdr0_pfc1);

		/*clear resets*/
		udelay (1000);
		mtsdr(SDR0_SRST1, 0x00000000);
		udelay (1000);
		mtsdr(SDR0_SRST0, 0x00000000);

		printf("USB:   Device(int phy)\n");
	}

	mfsdr(SDR0_SRST1, reg);		/* enable security/kasumi engines */
	reg &= ~(SDR0_SRST1_CRYP0 | SDR0_SRST1_KASU0);
	mtsdr(SDR0_SRST1, reg);

	/*
	 * Clear PLB4A0_ACR[WRP]
	 * This fix will make the MAL burst disabling patch for the Linux
	 * EMAC driver obsolete.
	 */
	reg = mfdcr(plb4_acr) & ~plb0_acr_wrp_2deep;
	mtdcr(plb4_acr, reg);

	return 0;
}

int checkboard(void)
{
	char *s = getenv("serial#");

	printf("Board: Roach");

	if (s != NULL) {
		puts(" serial=");
		puts(s);
	}
	putc('\n');

	return 0;
}

#if defined(CFG_DRAM_TEST)
int testdram(void)
{
	unsigned long *mem = (unsigned long *)0;
	const unsigned long kend = (1024 / sizeof(unsigned long));
	unsigned long k, n, r, st;

        r = gd->ram_size >> 20;
        printf("checking %lu MB\n", r);
	mtmsr(0);

	for (k = 0; k < r;
	     ++k, mem += (1024 / sizeof(unsigned long))) {
		if ((k & 1023) == 0) {
			printf("%3lu MB\r", k / 1024);
		}

		memset(mem, 0xaaaaaaaa, 1024);
		for (n = 0; n < kend; ++n) {
			if (mem[n] != 0xaaaaaaaa) {
				printf("SDRAM test fails at: %08x\n",
				       (uint) & mem[n]);
#if 1
                                mfsdram(DDR0_00, st);
                                printf("dram: reg 0:   0x%08lx\n", st);
                                mfsdram(DDR0_01, st);
                                printf("dram: reg 1:   0x%08lx\n", st);
                                printf("dram: content: 0x%08lx\n", mem[n]);
#endif
				return 1;
			}
		}

		memset(mem, 0x55555555, 1024);
		for (n = 0; n < kend; ++n) {
			if (mem[n] != 0x55555555) {
				printf("SDRAM test fails at: %08x\n",
				       (uint) & mem[n]);
				return 1;
			}
		}

                /* make the processor halt whenever it hits uninit ram, instruction jumps to itself */
		memset(mem, 0x48000000, 1024);
	}
	printf("SDRAM test passes\n");
	return 0;
}
#endif

#if defined(CONFIG_POST)
/*
 * Returns 1 if keys pressed to start the power-on long-running tests
 * Called from board_init_f().
 */
int post_hotkeys_pressed(void)
{
	return 0;	/* No hotkeys supported */
}
#endif /* CONFIG_POST */
