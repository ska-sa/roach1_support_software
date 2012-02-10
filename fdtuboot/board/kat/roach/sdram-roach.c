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
 * Marc Welz,               KAT, marc@ska.ac.za
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

#define ppcMsync      sync
#define ppcMbar       eieio

#define SPD_BYTES_USED                       0
#define SPD_BYTES_TOTAL                      1
#define SPD_MEMORY_TYPE                      2
#define SPD_ROW_ADDRESSES                    3
#define SPD_COLUMN_ADDRESSES                 4
#define SPD_RANKS                            5
#define SPD_TAC0                             9
#define SPD_MINC0               (SPD_MINC0 + 1) /* assumed by .c */
#define SPD_CONFIG_TYPE                     11
#define SPD_REFRESH_RATE                    12
#define SPD_BURST_LENGTHS                   16
#define SPD_BANKS                           17
#define SPD_CAS_LATENCIES                   18
#define SPD_DIMM_TYPE                       20
#define SPD_TAC1                            23
#define SPD_MINC1               (SPD_MINC1 + 1) /* assumed by .c */
#define SPD_TAC2                            25
#define SPD_MINC2               (SPD_MINC1 + 1) /* assumed by .c */
#define SPD_TRP                             27
#define SPD_TRRD                            28
#define SPD_TRCD                            29
#define SPD_TRAS                            30
#define SPD_DENSITY                         31
#define SPD_TWR                             36
#define SPD_TWTR                            37
#define SPD_TRTP                            38
#define SPD_TEXTRA                          40
#define SPD_TRC                             41
#define SPD_TRFC                            42
#define SPD_TCKMAX                          43
#define SPD_REVISION                        62

#define SPD_MEMORY_TYPE_DDR_SDRAM            7
#define SPD_MEMORY_TYPE_DDR2_SDRAM           8

#define SPD_BURST_LENGTH_4                 0x4
#define SPD_BURST_LENGTH_8                 0x8

#define SPD_TEXTRA_TRC_DECODE(p)          (((p) & 0x70) >> 4)
#define SPD_TEXTRA_TRFC_DECODE(p)         (((p) & 0x0d) >> 1)
#define SPD_TEXTRA_TRFC_MSB                0x1

#define SPD_CONFIG_TYPE_DATA_PARITY        0x1
#define SPD_CONFIG_TYPE_ECC                0x2
#define SPD_CONFIG_TYPE_CONTROL_PARITY     0x4

#define SPD_DIMM_RDIMM                     0x01
#define SPD_DIMM_MINI_RDIMM                0x10

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
			printf("(spd v1.%d) ", data[0] & 0xf);
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
phys_size_t initdram (int board_type)
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

	denali_wait_for_dlllock();

	mfsdram(DDR0_00, st);
	debug("dram: reg 0 after lock: 0x%08x\n", st);
	mfsdram(DDR0_01, st);
	debug("dram: reg 1 after lock: 0x%08x\n", st);

#ifdef CONFIG_DDR_DATA_EYE
	/* -----------------------------------------------------------+
	 * Perform data eye search if requested.
	 * ----------------------------------------------------------*/
	denali_core_search_data_eye();
#endif

#if 0
#define MAX_SUPPORTED_RAM 256  /* maps one entry, will work with default linux map */
#endif
#define MAX_SUPPORTED_RAM 2048 /* can only map 2G, kernel needs work to see more than 1G */

	set_mcsr(get_mcsr());

	if(sz > MAX_SUPPORTED_RAM){
		printf("dram: warning: cpu can only use %dM of %dM\n", MAX_SUPPORTED_RAM, sz);
		sz = MAX_SUPPORTED_RAM;
	}

	return (sz << 20);
}
