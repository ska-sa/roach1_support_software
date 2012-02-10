/*
 * (C) Copyright 2006
 * Sylvie Gohl,             AMCC/IBM, gohl.sylvie@fr.ibm.com
 * Jacqueline Pira-Ferriol, AMCC/IBM, jpira-ferriol@fr.ibm.com
 * Thierry Roman,           AMCC/IBM, thierry_roman@fr.ibm.com
 * Alain Saurel,            AMCC/IBM, alain.saurel@fr.ibm.com
 * Robert Snyder,           AMCC/IBM, rob.snyder@fr.ibm.com
 *
 * (C) Copyright 2006-2007
 * Stefan Roese, DENX Software Engineering, sr@denx.de.
 *
 * (C) Copyright 2008
 * Marc Welz,               KAT,      marc@ska.ac.za
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

/* define DEBUG for debug output */

#include <common.h>
#include <asm/processor.h>
#include <asm/io.h>
#include <ppc440.h>
#include <i2c.h>

#include "sdram.h"

#if defined(CONFIG_DDR_DATA_EYE)
/*-----------------------------------------------------------------------------+
 * wait_for_dlllock.
 +----------------------------------------------------------------------------*/
static int wait_for_dlllock(void)
{
	unsigned long val;
	int wait = 0;

	/* -----------------------------------------------------------+
	 * Wait for the DCC master delay line to finish calibration
	 * ----------------------------------------------------------*/
	mtdcr(ddrcfga, DDR0_17);
	val = DDR0_17_DLLLOCKREG_UNLOCKED;

	while (wait != 0xffff) {
		val = mfdcr(ddrcfgd);
		if ((val & DDR0_17_DLLLOCKREG_MASK) == DDR0_17_DLLLOCKREG_LOCKED)
			/* dlllockreg bit on */
			return 0;
		else
			wait++;
	}
	debug("0x%04x: DDR0_17 Value (dlllockreg bit): 0x%08x\n", wait, val);
	debug("Waiting for dlllockreg bit to raise\n");

	return -1;
}

/*-----------------------------------------------------------------------------+
 * wait_for_dram_init_complete.
 +----------------------------------------------------------------------------*/
int wait_for_dram_init_complete(void)
{
	unsigned long val;
	int wait = 0;

	/* --------------------------------------------------------------+
	 * Wait for 'DRAM initialization complete' bit in status register
	 * -------------------------------------------------------------*/
	mtdcr(ddrcfga, DDR0_00);

	while (wait != 0xffff) {
		val = mfdcr(ddrcfgd);
		if ((val & DDR0_00_INT_STATUS_BIT6) == DDR0_00_INT_STATUS_BIT6)
			/* 'DRAM initialization complete' bit */
			return 0;
		else
			wait++;
	}

	debug("DRAM initialization complete bit in status register did not rise\n");

	return -1;
}

unsigned int check_diff(u32 a, u32 b)
{
	u32 d;
	unsigned int result, i;

	d = a ^ b;
	result = 0;
	for(i = 0; i < 32; i++){
		result += ((d >> i) & 1);
	}

	return result;
}

#define NUM_TRIES 64
#define NUM_READS 10

/*-----------------------------------------------------------------------------+
 * denali_core_search_data_eye.
 +----------------------------------------------------------------------------*/

void denali_core_search_data_eye(unsigned long memory_size)
{
	/* WARNING: for debug purposes the logic has been changed to */
	/* return the least bad configuration, instead of just failing */ 
	/* For a real system this is bad, as it tries to run with */
	/* flaky RAM. Furhtermore the replacement search doesn't */
	/* place the system in the best contiguous region */

	int k, j;
	u32 val, wr_dqs_shift, dqs_out_shift, dll_dqs_delay_X;
	volatile u32 *ram_pointer;

        unsigned int best_error, this_error, best_sum, this_sum;
        unsigned int best_shift, best_delay, good_delay, better_delay, good_error, best_range, this_range;

	u32 test[NUM_TRIES] = {
		0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF,
		0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF,
		0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000,
		0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000,
		0xAAAAAAAA, 0xAAAAAAAA, 0x55555555, 0x55555555,
		0xAAAAAAAA, 0xAAAAAAAA, 0x55555555, 0x55555555,
		0x55555555, 0x55555555, 0xAAAAAAAA, 0xAAAAAAAA,
		0x55555555, 0x55555555, 0xAAAAAAAA, 0xAAAAAAAA,
		0xA5A5A5A5, 0xA5A5A5A5, 0x5A5A5A5A, 0x5A5A5A5A,
		0xA5A5A5A5, 0xA5A5A5A5, 0x5A5A5A5A, 0x5A5A5A5A,
		0x5A5A5A5A, 0x5A5A5A5A, 0xA5A5A5A5, 0xA5A5A5A5,
		0x5A5A5A5A, 0x5A5A5A5A, 0xA5A5A5A5, 0xA5A5A5A5,
		0xAA55AA55, 0xAA55AA55, 0x55AA55AA, 0x55AA55AA,
		0xAA55AA55, 0xAA55AA55, 0x55AA55AA, 0x55AA55AA,
		0x55AA55AA, 0x55AA55AA, 0xAA55AA55, 0xAA55AA55,
		0x55AA55AA, 0x55AA55AA, 0xAA55AA55, 0xAA55AA55 };

	ram_pointer = (volatile u32 *)(CFG_SDRAM_BASE);

        /* keep -Wall happy */

        best_sum   = 0xffffffff; /* per shift error totals */
        best_error = 0xffffffff; /* individual delay errors */

        best_shift = 0; /* TODO should be the saved value */
        best_delay = 0; /* TODO should be the saved value */

	for (wr_dqs_shift = 64; wr_dqs_shift < 96; wr_dqs_shift++) {
		/*for (wr_dqs_shift=1; wr_dqs_shift<96; wr_dqs_shift++) {*/

		/* -----------------------------------------------------------+
		 * De-assert 'start' parameter.
		 * ----------------------------------------------------------*/
		mtdcr(ddrcfga, DDR0_02);
		val = (mfdcr(ddrcfgd) & ~DDR0_02_START_MASK) | DDR0_02_START_OFF;
		mtdcr(ddrcfgd, val);

		/* -----------------------------------------------------------+
		 * Set 'wr_dqs_shift'
		 * ----------------------------------------------------------*/
		mtdcr(ddrcfga, DDR0_09);
		val = (mfdcr(ddrcfgd) & ~DDR0_09_WR_DQS_SHIFT_MASK)
			| DDR0_09_WR_DQS_SHIFT_ENCODE(wr_dqs_shift);
		mtdcr(ddrcfgd, val);

		/* -----------------------------------------------------------+
		 * Set 'dqs_out_shift' = wr_dqs_shift + 32
		 * ----------------------------------------------------------*/
		dqs_out_shift = wr_dqs_shift + 32;
		mtdcr(ddrcfga, DDR0_22);
		val = (mfdcr(ddrcfgd) & ~DDR0_22_DQS_OUT_SHIFT_MASK)
			| DDR0_22_DQS_OUT_SHIFT_ENCODE(dqs_out_shift);
		mtdcr(ddrcfgd, val);

                good_delay = 0;
                better_delay = 0;

                best_range = 0;
                this_range = 0;
                good_error = 0xffffffff;
                this_sum = 0;

		for (dll_dqs_delay_X = 1; dll_dqs_delay_X < 64; dll_dqs_delay_X++) {
		/*for (dll_dqs_delay_X=1; dll_dqs_delay_X<128; dll_dqs_delay_X++) */
			/* -----------------------------------------------------------+
			 * Set 'dll_dqs_delay_X'.
			 * ----------------------------------------------------------*/
			/* dll_dqs_delay_0 */
			mtdcr(ddrcfga, DDR0_17);
			val = (mfdcr(ddrcfgd) & ~DDR0_17_DLL_DQS_DELAY_0_MASK)
				| DDR0_17_DLL_DQS_DELAY_0_ENCODE(dll_dqs_delay_X);
			mtdcr(ddrcfgd, val);
			/* dll_dqs_delay_1 to dll_dqs_delay_4 */
			mtdcr(ddrcfga, DDR0_18);
			val = (mfdcr(ddrcfgd) & ~DDR0_18_DLL_DQS_DELAY_X_MASK)
				| DDR0_18_DLL_DQS_DELAY_4_ENCODE(dll_dqs_delay_X)
				| DDR0_18_DLL_DQS_DELAY_3_ENCODE(dll_dqs_delay_X)
				| DDR0_18_DLL_DQS_DELAY_2_ENCODE(dll_dqs_delay_X)
				| DDR0_18_DLL_DQS_DELAY_1_ENCODE(dll_dqs_delay_X);
			mtdcr(ddrcfgd, val);
			/* dll_dqs_delay_5 to dll_dqs_delay_8 */
			mtdcr(ddrcfga, DDR0_19);
			val = (mfdcr(ddrcfgd) & ~DDR0_19_DLL_DQS_DELAY_X_MASK)
				| DDR0_19_DLL_DQS_DELAY_8_ENCODE(dll_dqs_delay_X)
				| DDR0_19_DLL_DQS_DELAY_7_ENCODE(dll_dqs_delay_X)
				| DDR0_19_DLL_DQS_DELAY_6_ENCODE(dll_dqs_delay_X)
				| DDR0_19_DLL_DQS_DELAY_5_ENCODE(dll_dqs_delay_X);
			mtdcr(ddrcfgd, val);

			ppcMsync();
			ppcMbar();

			/* -----------------------------------------------------------+
			 * Assert 'start' parameter.
			 * ----------------------------------------------------------*/
			mtdcr(ddrcfga, DDR0_02);
			val = (mfdcr(ddrcfgd) & ~DDR0_02_START_MASK) | DDR0_02_START_ON;
			mtdcr(ddrcfgd, val);

			ppcMsync();
			ppcMbar();

			/* -----------------------------------------------------------+
			 * Wait for the DCC master delay line to finish calibration
			 * ----------------------------------------------------------*/
			if (wait_for_dlllock() != 0) {
				printf("dlllock did not occur !!!\n");
				printf("denali_core_search_data_eye!!!\n");
				printf("wr_dqs_shift = %d - dll_dqs_delay_X = %d\n",
				       wr_dqs_shift, dll_dqs_delay_X);
				hang();
			}
			ppcMsync();
			ppcMbar();

			if (wait_for_dram_init_complete() != 0) {
				printf("dram init complete did not occur !!!\n");
				printf("denali_core_search_data_eye!!!\n");
				printf("wr_dqs_shift = %d - dll_dqs_delay_X = %d\n",
				       wr_dqs_shift, dll_dqs_delay_X);
				hang();
			}
			udelay(100);  /* wait 100us to ensure init is really completed !!! */

			/* write values */
			for (j=0; j<NUM_TRIES; j++) {
				ram_pointer[j] = test[j];

				/* clear any cache at ram location */
				__asm__("dcbf 0,%0": :"r" (&ram_pointer[j]));
			}

                        this_error = 0;

			/* read values back */
			for (j=0; j<NUM_TRIES; j++) {
				for (k=0; k<NUM_READS; k++) {
					/* clear any cache at ram location */
					__asm__("dcbf 0,%0": :"r" (&ram_pointer[j]));

                                        this_error += check_diff(ram_pointer[j], test[j]);
				}

			}

			if(this_error <= good_error){
				good_error = this_error;
				if(this_range == 0){
					good_delay = dll_dqs_delay_X;
				}
				this_range++;
				if(this_range > best_range){
					better_delay = good_delay;
					best_range = this_range;
				}
#if 0
				debug("update: delay=%d, shift=%d errors=%u (range=%d,%d)\n", dll_dqs_delay_X, wr_dqs_shift, this_error, this_range, best_range);
#endif
			} else {
				this_range = 0;
			}

			this_sum += this_error;

			/* -----------------------------------------------------------+
			 * De-assert 'start' parameter.
			 * ----------------------------------------------------------*/
			mtdcr(ddrcfga, DDR0_02);
			val = (mfdcr(ddrcfgd) & ~DDR0_02_START_MASK) | DDR0_02_START_OFF;
			mtdcr(ddrcfgd, val);

		} /* for (dll_dqs_delay_X=0; dll_dqs_delay_X<128; dll_dqs_delay_X++) */

		if(good_error < best_error){
			best_error = good_error;
			best_delay = better_delay + (best_range / 2);
			best_shift = wr_dqs_shift;
		} else if(this_sum < best_sum){
			best_sum = this_sum;
			best_delay = better_delay + (best_range / 2);
			best_shift = wr_dqs_shift;
		}

	} /* for (wr_dqs_shift=0; wr_dqs_shift<96; wr_dqs_shift++) */

	/* -----------------------------------------------------------+
	 * Largest passing window is now detected.
	 * ----------------------------------------------------------*/

	/* Compute dll_dqs_delay_X value */
#if 0
	dll_dqs_delay_X = (dll_dqs_delay_X_end_window + dll_dqs_delay_X_start_window) / 2;
#endif

	debug("DQS calibration - Window detected:\n");
	debug("best shift      = %d\n", best_shift);
	debug("best_delay      = %d\n", best_delay);

	dll_dqs_delay_X = best_delay;
	wr_dqs_shift = best_shift;

#if 0
	debug("dll_dqs_delay_X window = %d - %d\n",
	       dll_dqs_delay_X_start_window, dll_dqs_delay_X_end_window);
#endif

	/* -----------------------------------------------------------+
	 * De-assert 'start' parameter.
	 * ----------------------------------------------------------*/
	mtdcr(ddrcfga, DDR0_02);
	val = (mfdcr(ddrcfgd) & ~DDR0_02_START_MASK) | DDR0_02_START_OFF;
	mtdcr(ddrcfgd, val);

	/* -----------------------------------------------------------+
	 * Set 'wr_dqs_shift'
	 * ----------------------------------------------------------*/
	mtdcr(ddrcfga, DDR0_09);
	val = (mfdcr(ddrcfgd) & ~DDR0_09_WR_DQS_SHIFT_MASK)
		| DDR0_09_WR_DQS_SHIFT_ENCODE(wr_dqs_shift);
	mtdcr(ddrcfgd, val);
	debug("DDR0_09=0x%08lx\n", val);

	/* -----------------------------------------------------------+
	 * Set 'dqs_out_shift' = wr_dqs_shift + 32
	 * ----------------------------------------------------------*/
	dqs_out_shift = wr_dqs_shift + 32;
	mtdcr(ddrcfga, DDR0_22);
	val = (mfdcr(ddrcfgd) & ~DDR0_22_DQS_OUT_SHIFT_MASK)
		| DDR0_22_DQS_OUT_SHIFT_ENCODE(dqs_out_shift);
	mtdcr(ddrcfgd, val);
	debug("DDR0_22=0x%08lx\n", val);

	/* -----------------------------------------------------------+
	 * Set 'dll_dqs_delay_X'.
	 * ----------------------------------------------------------*/
	/* dll_dqs_delay_0 */
	mtdcr(ddrcfga, DDR0_17);
	val = (mfdcr(ddrcfgd) & ~DDR0_17_DLL_DQS_DELAY_0_MASK)
		| DDR0_17_DLL_DQS_DELAY_0_ENCODE(dll_dqs_delay_X);
	mtdcr(ddrcfgd, val);
	debug("DDR0_17=0x%08lx\n", val);

	/* dll_dqs_delay_1 to dll_dqs_delay_4 */
	mtdcr(ddrcfga, DDR0_18);
	val = (mfdcr(ddrcfgd) & ~DDR0_18_DLL_DQS_DELAY_X_MASK)
		| DDR0_18_DLL_DQS_DELAY_4_ENCODE(dll_dqs_delay_X)
		| DDR0_18_DLL_DQS_DELAY_3_ENCODE(dll_dqs_delay_X)
		| DDR0_18_DLL_DQS_DELAY_2_ENCODE(dll_dqs_delay_X)
		| DDR0_18_DLL_DQS_DELAY_1_ENCODE(dll_dqs_delay_X);
	mtdcr(ddrcfgd, val);
	debug("DDR0_18=0x%08lx\n", val);

	/* dll_dqs_delay_5 to dll_dqs_delay_8 */
	mtdcr(ddrcfga, DDR0_19);
	val = (mfdcr(ddrcfgd) & ~DDR0_19_DLL_DQS_DELAY_X_MASK)
		| DDR0_19_DLL_DQS_DELAY_8_ENCODE(dll_dqs_delay_X)
		| DDR0_19_DLL_DQS_DELAY_7_ENCODE(dll_dqs_delay_X)
		| DDR0_19_DLL_DQS_DELAY_6_ENCODE(dll_dqs_delay_X)
		| DDR0_19_DLL_DQS_DELAY_5_ENCODE(dll_dqs_delay_X);
	mtdcr(ddrcfgd, val);
	debug("DDR0_19=0x%08lx\n", val);

	/* -----------------------------------------------------------+
	 * Assert 'start' parameter.
	 * ----------------------------------------------------------*/
	mtdcr(ddrcfga, DDR0_02);
	val = (mfdcr(ddrcfgd) & ~DDR0_02_START_MASK) | DDR0_02_START_ON;
	mtdcr(ddrcfgd, val);

	ppcMsync();
	ppcMbar();

	/* -----------------------------------------------------------+
	 * Wait for the DCC master delay line to finish calibration
	 * ----------------------------------------------------------*/
	if (wait_for_dlllock() != 0) {
		printf("dram: dlllock did not occur\n");
		hang();
	}
	ppcMsync();
	ppcMbar();

	if (wait_for_dram_init_complete() != 0) {
		printf("dram: init complete did not occur\n");
		hang();
	}
	udelay(100);  /* wait 100us to ensure init is really completed !!! */
}
#endif /* CONFIG_DDR_DATA_EYE */

/*************************************************************************
 *
 * functions to extract setting from DIMM and program the DENALI Core
 *
 ************************************************************************/

static int check_sane_spd(unsigned int chip)
{
	unsigned char data[2];

	if(i2c_probe(chip)){
		return -1;
	}

	if(i2c_read(chip, SPD_BYTES_USED, 1, data, 1) || (data[0] < 64)){
		printf("dram: missing spd data\n");
		return -1;
	}

	if(i2c_read(chip, SPD_REVISION, 1, data, 1) == 0){
		if((data[0] & 0xf0) == 0x10){
			printf("dram: spd revision 1.%d\n", data[0] & 0xf);
		} else {
			printf("dram: bad or unknown spd code 0x%x\n", data[0]);
			return -1;
		}
	}

	return 0;
}

#ifndef DEBUG
#define xmtsdram(p,q)     mtsdram(p,q)
#define ps2cycles(p,q,r)  dops2cycles(p,q)
static unsigned int dops2cycles(unsigned long speed, unsigned int ps)
{
	return (((speed / 1000000) * (ps / 10)) / 100000);
}
#else
#define xmtsdram(p,q)     printf("dram: 0x%x = 0x%08x\n", p, q); mtsdram(p, q)
#define ps2cycles(p,q,r)  dops2cycles(p,q,r)
static unsigned int dops2cycles(unsigned long speed, unsigned int ps, char *field)
{
	unsigned int result;

	result = (((speed / 1000000) * (ps / 10)) / 100000);

	printf("dram %s: %ups@%luHz -> %u\n", field, ps, speed, result);

	return result;
}
#endif

unsigned int extra_fractions(unsigned int extra)
{
	switch(extra){
		case 0x1 : return 250;
		case 0x2 : return 333;
		case 0x3 : return 500;
		case 0x4 : return 666;
		case 0x5 : return 750;
		/* case 0 */
		default  : return 0;
	}
}

unsigned int decode_refresh(unsigned int value)
{
	unsigned int result;

	result = 15625000;

	switch(value){
		case 0x81 : return result / 4;
		case 0x82 : return result / 2;
		case 0x83 : return result * 2;
		case 0x84 : return result * 4;
		case 0x85 : return result * 8;
		default : return result;
	}
}

/*************************************************************************
 *
 * initdram -- 440EPx's DDR controller is a DENALI Core
 *
 ************************************************************************/

long int initdram (int board_type)
{
	unsigned char data[2];
	unsigned int chip, v, t, i, x, sz, ms, rr, rf, st;
	unsigned long speed;
	unsigned char mt, bl, cl, rk, tmp; /* memory type, burst length, cas latency, ranks */
	unsigned char map[3] = { SPD_TAC0, SPD_TAC1, SPD_TAC2 };
#ifdef DEBUG
	unsigned int j;
#endif

	chip = SPD_EEPROM_ADDRESS;

	debug("dram: about to probe chip 0x%x\n", chip);

	if(check_sane_spd(chip)){
		printf("dram: nonexistent or unusable spd eeprom\n");
		hang();
	}

#ifdef DEBUG
	debug("dram: raw");
	for(j = 0; j < 64; j++){
		if(i2c_read(chip, j, 1, data, 1) == 0){
			debug("%c%02x", (j % 16) ? ' ' : '\n', data[0]);
		}
	}
	debug("\n");
#endif

	/* rushed dump from spd to ddr controller, calculations hardwired from sequoia and generally iffy */

	if(i2c_read(chip, SPD_MEMORY_TYPE, 1, data, 1) != 0){
		return 0;
	}
	mt = data[0];

	switch(mt){
		case SPD_MEMORY_TYPE_DDR2_SDRAM :
			debug("dram: ddr2 memory\n");
			break;
		/* case SPD_MEMORY_TYPE_DDR_SDRAM  : */
		default :
			printf("dram: unsupported memory type 0x%x\n", mt);
			return 0;
	}

	speed = get_bus_freq(0);
	debug("dram: bus speed %luHz\n", speed);

	if(i2c_read(chip, SPD_REFRESH_RATE, 1, data, 1) != 0){
		return 0;
	}
	rr = decode_refresh(data[0]);
	debug("dram: refresh rate %ups\n", rr);

	xmtsdram(DDR0_02, DDR0_02_START_OFF);
	mfsdram(DDR0_02, ms);
	/* gives us the maximum dimensions the controller can do, used later */
	debug("dram: controller caps 0x%08x\n", ms);

	/* calibration values as recommended */
	xmtsdram(DDR0_00, DDR0_00_DLL_INCREMENT_ENCODE(0x19) | DDR0_00_DLL_START_POINT_ENCODE(0xa));

	/* set as required, possibly could set up interrupt masks */
	xmtsdram(DDR0_01, DDR0_01_PLB0_DB_CS_LOWER_ENCODE(0x1) | DDR0_01_PLB0_DB_CS_UPPER_ENCODE(0));

	v = 0;
	v |= DDR0_03_INITAREF_ENCODE(0x2); /* WARNING: no idea how many autorefresh commands needed during initialisation */
	if(i2c_read(chip, SPD_BURST_LENGTHS, 1, data, 1) != 0){
		return 0;
	}
	bl = ((mt != SPD_MEMORY_TYPE_DDR2_SDRAM) && (data[0] & SPD_BURST_LENGTH_8)) ? 8 : 4;
	debug("dram: burst length caps=0x%x, chosen=%d\n", data[0], bl);
	v |= DDR0_03_BSTLEN_ENCODE((bl == 8) ? 0x3 : 0x2);
	if(i2c_read(chip, SPD_CAS_LATENCIES, 1, data, 1) != 0){
		return 0;
	}
	cl = 0;
	tmp = data[0];
	debug("dram: latency choices 0x%x\n", tmp);
	/* could use a less agressive mode by quitting for a lower x */
	for(i = 7, x = 0; (i >= 2) && (x < 3); i--){
		if(tmp & (0x1 << i)){
			debug("dram: can do cl=%d\n", i);
			if(i2c_read(chip, map[x] + 1, 1, data, 1) != 0){
				return 0;
			}
			t = ps2cycles(speed, (((data[0] >> 4) & 0xf) * 100) + ((data[0] & 0xf) * 10), "minclock");
			if(t > 0){
				debug("dram: clock too fast for cl-%d\n", x);
				break;
			}
			cl = i;
			x++;
		}
	}
	if((cl < 2) || (cl > 5)){
		printf("dram: no suitable cas found in mask 0x%x\n", data[0]);
		return 0;
	}
	x--;
	debug("dram: selected cas=%d, X-%d\n", cl, x);
	v |= DDR0_03_CASLAT_ENCODE(cl); /* WARNING: guessing CASLAT value, not sure what the register expects */
	v |= DDR0_03_CASLAT_LIN_ENCODE(cl << 1);
	xmtsdram(DDR0_03, v);

	v = 0;
	if(i2c_read(chip, SPD_TEXTRA, 1, data, 1) != 0){
		return 0;
	}
	t = extra_fractions(SPD_TEXTRA_TRC_DECODE(data[0]));
	if(i2c_read(chip, SPD_TRC, 1, data, 1) != 0){
		return 0;
	}
	t = ps2cycles(speed, (data[0] * 1000) + t, "trc");
	v |= DDR0_04_TRC_ENCODE(t + 1);
	if(i2c_read(chip, SPD_TRRD, 1, data, 1) != 0){
		return 0;
	}
	t = ps2cycles(speed, data[0] * 250, "trrd");
	v |= DDR0_04_TRRD_ENCODE(t + 1);
	if(i2c_read(chip, SPD_TRTP, 1, data, 1) != 0){
		return 0;
	}
	t = ps2cycles(speed, data[0] * 250, "trtp");
	v |= DDR0_04_TRTP_ENCODE(t + 1);
	xmtsdram(DDR0_04, v);

	v = 0;
	/* WARNING: copying TMRD and TERMS blindly */
	v |= DDR0_05_TMRD_ENCODE(0x2) | DDR0_05_TEMRS_ENCODE(0x2);
	if(i2c_read(chip, SPD_TRP, 1, data, 1) != 0){
		return 0;
	}
	t = ps2cycles(speed, data[0] * 250, "trp");
	v |= DDR0_05_TRP_ENCODE(t + 1);
	if(i2c_read(chip, SPD_TRAS, 1, data, 1) != 0){
		return 0;
	}
	t = ps2cycles(speed, data[0] * 1000, "tras");
	v |= DDR0_05_TRAS_MIN_ENCODE(t + 1);
	xmtsdram(DDR0_05, v);

	v = 0;
	v |=  DDR0_06_TDLL_ENCODE(0xC8); /* WARNING: copied blindly, could be ppl relock ? */
	v |= DDR0_06_WRITEINTERP_ENCODE(0x1); /* NOTE: won't interrupt writes yet */
	if(i2c_read(chip, SPD_TWTR, 1, data, 1) != 0){
		return 0;
	}
	t = ps2cycles(speed, data[0] * 250, "twtr");
	v |= DDR0_06_TWTR_ENCODE(t + 1);
	if(i2c_read(chip, SPD_TEXTRA, 1, data, 1) != 0){
		return 0;
	}
	t = extra_fractions(SPD_TEXTRA_TRFC_DECODE(data[0]));
	if(data[0] & SPD_TEXTRA_TRFC_MSB){
		t += 256000;
	}
	if(i2c_read(chip, SPD_TRFC, 1, data, 1) != 0){
		return 0;
	}
	rf = (data[0] * 1000) + t;
	t = ps2cycles(speed, rf, "trfc");
	v |= DDR0_06_TRFC_ENCODE(t + 1);
	xmtsdram(DDR0_06, v);

	v = 0;
	v |= DDR0_07_TFAW_ENCODE(0x0D); /* WARNING: blind copy */
	v |= DDR0_07_AUTO_REFRESH_MODE_ENCODE(1); /* refresh on boundaries */
	v |= DDR0_07_AREFRESH_ENCODE(1); /* enable autorefresh */
	xmtsdram(DDR0_07, v);

	v = 0;
	v |= DDR0_08_WRLAT_ENCODE(cl - 1); /* apparently only guarenteed to be the case for DDR2 */
	v |= DDR0_08_TCPD_ENCODE(200); /* the datasheet says if in doubt use 200, I am in doubt */
	v |= DDR0_08_DDRII_ENCODE(((mt ==  SPD_MEMORY_TYPE_DDR2_SDRAM) ? 1 : 0));
	xmtsdram(DDR0_08, v);

	v = 0;
	v |= DDR0_09_RTT_0_ENCODE(0x01); /* 75 Ohm */
	v |= DDR0_09_WR_DQS_SHIFT_BYPASS_ENCODE(0x1D); /* copied blindly */
	v |= DDR0_09_WR_DQS_SHIFT_ENCODE(0x5F); /* copied blindly */
	xmtsdram(DDR0_09, v);

	v = 0;
	if(i2c_read(chip, SPD_RANKS, 1, data, 1) != 0){
		return 0;
	}
	rk = (data[0] & 0x7) + 1;
	debug("dram: %d ranks\n", rk);
	v |= DDR0_10_CS_MAP_ENCODE((rk == 1) ? 0x1 : 0x3);
	xmtsdram(DDR0_10, v);

	if(i2c_read(chip, SPD_DENSITY, 1, data, 1) != 0){
		return 0;
	}
	t = (((data[0] << 3) & 0xf8) | ((data[0] >> 5) & 0x7)) * 128;
	debug("dram: each rank %dMb\n", t);
	sz = ((rk == 1) ? 1 : 2) * t;

	v = 0;
	t = ps2cycles(speed, rf + 10000, "txsnr");
	v |= DDR0_11_TXSNR_ENCODE(t + 1);
	v |= DDR0_11_TXSR_ENCODE(200); /* as per ddr2 ? */
	xmtsdram(DDR0_11, v);

	v = 0;
	v |= DDR0_12_TCKE_ENCODE(3); /* seems fixed for ddr2 */
	if(i2c_read(chip, SPD_TCKMAX, 1, data, 1) != 0){
		return 0;
	}
	/* the kind of statement your mother warned you about */
	t = (((data[0] >> 4) & 0xf) * 1000) + ((data[0] & 0x8) ? (((data[0] >> 1) & 0x3) * (1000 / (3 + ((data[0] ^ (data[0] >> 1)) & 0x1)))) : ((data[0] & 0x7) * 100));
	/* WARNING t isn't actually used yet */
	debug("dram: tckmax is %dps, arb tcke set to 3\n", t);
	xmtsdram(DDR0_12, v);

	/* no DDR0_13 */

	v = 0;
	v |= DDR0_14_DLL_BYPASS_MODE_ENCODE(0);
	v |= DDR0_14_REDUC_ENCODE(0);
	if(i2c_read(chip, SPD_DIMM_TYPE, 1, data, 1) != 0){
		return 0;
	}
	t = ((data[0] == SPD_DIMM_RDIMM) || (data[0] == SPD_DIMM_MINI_RDIMM)) ? 1 : 0;
	debug("dram: memory is %s\n", t ? "registered" : "probably unbuffered");
	v |= DDR0_14_REG_DIMM_ENABLE_ENCODE(t);
	xmtsdram(DDR0_14, v);

	/* no DDR0_15, DDR0_16 */

	v = 0;
	v |= DDR0_17_DLL_DQS_DELAY_0_ENCODE(0x19); /* nominal value as per datasheet */
	xmtsdram(DDR0_17, v);

	v = 0;
	v |= DDR0_18_DLL_DQS_DELAY_1_ENCODE(0x19); /* more nominal values */
	v |= DDR0_18_DLL_DQS_DELAY_2_ENCODE(0x19); 
	v |= DDR0_18_DLL_DQS_DELAY_3_ENCODE(0x19);
	v |= DDR0_18_DLL_DQS_DELAY_4_ENCODE(0x19);
	xmtsdram(DDR0_18, v);

	v = 0;
	v |= DDR0_19_DLL_DQS_DELAY_5_ENCODE(0x19); /* more nominal values */
	v |= DDR0_19_DLL_DQS_DELAY_6_ENCODE(0x19); 
	v |= DDR0_19_DLL_DQS_DELAY_7_ENCODE(0x19);
	v |= DDR0_19_DLL_DQS_DELAY_8_ENCODE(0x19);
	xmtsdram(DDR0_19, v);

	v = 0;
	v |= DDR0_20_DLL_DQS_BYPASS_0_ENCODE(0xb); /* copied, fractions of a cycle */
	v |= DDR0_20_DLL_DQS_BYPASS_1_ENCODE(0xb); 
	v |= DDR0_20_DLL_DQS_BYPASS_2_ENCODE(0xb); 
	v |= DDR0_20_DLL_DQS_BYPASS_3_ENCODE(0xb); 
	xmtsdram(DDR0_20, v);

	v = 0;
	v |= DDR0_21_DLL_DQS_BYPASS_4_ENCODE(0xb); /* copied, fractions of a cycle */
	v |= DDR0_21_DLL_DQS_BYPASS_5_ENCODE(0xb); 
	v |= DDR0_21_DLL_DQS_BYPASS_6_ENCODE(0xb); 
	v |= DDR0_21_DLL_DQS_BYPASS_7_ENCODE(0xb); 
	xmtsdram(DDR0_21, v);

	v = 0;
	v |= DDR0_22_DLL_DQS_BYPASS_8_ENCODE(0xb); /* copied, fractions of a cycle */
	if(i2c_read(chip, SPD_CONFIG_TYPE, 1, data, 1) != 0){
		return 0;
	}
	if(data[0] & SPD_CONFIG_TYPE_ECC){
		printf("dram: notice: ecc ignored\n");
		v |= DDR0_22_CTRL_RAW_ENCODE(DDR0_22_CTRL_RAW_ECC_DISABLE); /* odd, expected no ram, but example code uses disable */
	} else {
		v |= DDR0_22_CTRL_RAW_ENCODE(DDR0_22_CTRL_RAW_NO_ECC_RAM); /* no ecc there */
	}
	v |= DDR0_22_DQS_OUT_SHIFT_BYPASS_ENCODE(0x26); /* copied blindly */
	v |= DDR0_22_DQS_OUT_SHIFT_ENCODE(0x7f);   /* nominal value */
	xmtsdram(DDR0_22, v);

	v = 0;
	v |= DDR0_23_ODT_RD_MAP_CS0_ENCODE(0); /* recommendation to be kept 0 */
	xmtsdram(DDR0_23, v);

	v = 0;
	v |= DDR0_24_RTT_PAD_TERMINATION_ENCODE(1); /* 75 Ohm recommended */ 
	v |= DDR0_24_ODT_RD_MAP_CS1_ENCODE(0); /* see cs0 above */
	v |= DDR0_24_ODT_WR_MAP_CS1_ENCODE(1);
	v |= DDR0_24_ODT_WR_MAP_CS0_ENCODE((rk == 1) ? 1 : 2); /* unsure here */
#if 0
	v |= DDR0_24_ODT_WR_MAP_CS0_ENCODE(2); /* test alternative */
#endif        
	xmtsdram(DDR0_24, v);

	/* DDR0_25 readonly */

	/* these will have to be computed */
	v = 0;
	/* blind copy, again */
	v |= DDR0_26_TRAS_MAX_ENCODE(0x5b26);
	t = ps2cycles(speed, rr, "tref");
	v |= DDR0_26_TREF_ENCODE(t + 1); /* WARNING: was 0x408, or 0x50c for faster clocks */
	xmtsdram(DDR0_26, v);

	v = 0;
	v |= DDR0_27_EMRS_DATA_ENCODE(0x800); /* suggestion by dave, otherwise can be left empty */
	v |= DDR0_27_TINIT_ENCODE(0x682b); /* blind copy */
	xmtsdram(DDR0_27, v);

	v = 0;
	v |= DDR0_28_EMRS3_DATA_ENCODE(0); /* left empty */
	v |= DDR0_28_EMRS2_DATA_ENCODE(0); /* left empty */
	xmtsdram(DDR0_28, v); 

	/* no DDR0_29, DDR0_30 */

	v = 0;
	v |= DDR0_31_XOR_CHECK_BITS_ENCODE(0); /* pattern to use in tests */
	xmtsdram(DDR0_31, v); 

	/* NO ecc error checking using registers DDR0_31 - DDR0_41 */

	v = 0;
	v |= DDR0_42_CASLAT_LIN_GATE_ENCODE(cl << 1); /* simply copied value */
	if(i2c_read(chip, SPD_ROW_ADDRESSES, 1, data, 1) != 0){
		return 0;
	}
	t = data[0] & 0x1f;
	debug("dram: got %d row addresses, limit %d\n", t, DDR0_02_MAX_ROW_REG_DECODE(ms));
	if(DDR0_02_MAX_ROW_REG_DECODE(ms) <= t){
		t = 0;
	} else {
		t = DDR0_02_MAX_ROW_REG_DECODE(ms) - t;
	}
	v |= DDR0_42_ADDR_PINS_ENCODE(t);
	xmtsdram(DDR0_42, v);

	v = 0;
	v |= DDR0_43_APREBIT_ENCODE(10);
	if(i2c_read(chip, SPD_BANKS, 1, data, 1) != 0){
		return 0;
	}
	debug("dram: have %d banks\n", data[0]);
	switch(data[0]){
		case 0x4 :
			v |= DDR0_43_EIGHT_BANK_MODE_ENCODE(DDR0_43_EIGHT_BANK_MODE_4_BANKS);
			break;
		case 0x8 :
			v |= DDR0_43_EIGHT_BANK_MODE_ENCODE(DDR0_43_EIGHT_BANK_MODE_8_BANKS);
			break;
		default :
			printf("dram: %d banks unsupported\n", data[0]);
			return 0;
	}
	if(i2c_read(chip, SPD_COLUMN_ADDRESSES, 1, data, 1) != 0){
		return 0;
	}
	t = data[0] & 0x1f;
	debug("dram: got %d column addresses, limit %d\n", t, DDR0_02_MAX_COL_REG_DECODE(ms));
	if(DDR0_02_MAX_COL_REG_DECODE(ms) <= t){
		t = 0;
	} else {
		t = DDR0_02_MAX_COL_REG_DECODE(ms) - t;
	}
	v |= DDR0_43_COLUMN_SIZE_ENCODE(t);
	if(i2c_read(chip, SPD_TWR, 1, data, 1) != 0){
		return 0;
	}
	t = ps2cycles(speed, data[0] * 250, "twr");
	v |= DDR0_43_TWR_ENCODE(t + 1);
	xmtsdram(DDR0_43, v);

	v = 0;
	if(i2c_read(chip, SPD_TRCD, 1, data, 1) != 0){
		return 0;
	}
	t = ps2cycles(speed, data[0] * 250, "trcd");
	v |= DDR0_44_TRCD_ENCODE(t + 1);
	xmtsdram(DDR0_44, v);

	debug("dram: making memory go\n");
	xmtsdram(DDR0_02, DDR0_02_START_ON);
	wait_for_dlllock();

	mfsdram(DDR0_00, st);
	debug("dram: reg 0 after lock: 0x%08x\n", st);
	mfsdram(DDR0_01, st);
	debug("dram: reg 1 after lock: 0x%08x\n", st);

#ifdef CONFIG_DDR_DATA_EYE
	/* -----------------------------------------------------------+
	 * Perform data eye search if requested.
	 * ----------------------------------------------------------*/
	denali_core_search_data_eye(sz << 20);
#endif

#if 0
#define MAX_SUPPORTED_RAM 256  /* maps one entry, will work with default linux map */
#endif
#define MAX_SUPPORTED_RAM 2048 /* can only map 2G, kernel needs work to see more than 1G */

	if(sz > MAX_SUPPORTED_RAM){
		printf("dram: warning: cpu can only use %dM of %dM\n", MAX_SUPPORTED_RAM, sz);
		sz = MAX_SUPPORTED_RAM;
	}

	return (sz << 20);
}
