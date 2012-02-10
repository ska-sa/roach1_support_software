/*
 * (C) Copyright 2006-2007
 * Stefan Roese, DENX Software Engineering, sr@denx.de.
 *
 * (C) Copyright 2006
 * Jacqueline Pira-Ferriol, AMCC/IBM, jpira-ferriol@fr.ibm.com
 * Alain Saurel,            AMCC/IBM, alain.saurel@fr.ibm.com
 *
 * (C) Copyright 2008
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

/************************************************************************
 * roach.h - configuration for roach board
 ***********************************************************************/
#ifndef __CONFIG_H
#define __CONFIG_H

/*-----------------------------------------------------------------------
 * High Level Configuration Options
 *----------------------------------------------------------------------*/
#define CONFIG_ROACH     	1		/* Board is Roach	*/
#define ROACH_V1_01
#define CONFIG_440EPX		1		/* Specific PPC440EPx	*/
#define CONFIG_4xx		1		/* ... PPC4xx family	*/
#define CONFIG_SYS_CLK_FREQ	33000000	/* external freq to pll	*/

#define CONFIG_BOARD_EARLY_INIT_F 1		/* Call board_early_init_f */
#define CONFIG_MISC_INIT_R	1		/* Call misc_init_r	*/

/*-----------------------------------------------------------------------
 * Base addresses -- Note these are effective addresses where the
 * actual resources get mapped (not physical addresses)
 *----------------------------------------------------------------------*/
#define CFG_MONITOR_LEN		(384 * 1024)	/* Reserve 384 kB for Monitor	*/
#define CFG_MALLOC_LEN		(256 * 1024)	/* Reserve 256 kB for malloc()	*/

#define CFG_BOOT_BASE_ADDR	0xf0000000

/* CPU can't see more than 2G */
#define CFG_SDRAM_BASE		0x00000000	/* _must_ be 0		*/
#define CFG_SDRAM_BASE1		0x10000000	/* base +  256M  	*/
#define CFG_SDRAM_BASE2		0x20000000	/* base +  512M  	*/
#define CFG_SDRAM_BASE3		0x30000000	/* base +  768M  	*/
#define CFG_SDRAM_BASE4		0x40000000	/* base + 1024M  	*/
#define CFG_SDRAM_BASE5		0x50000000	/* base + 1280M  	*/
#define CFG_SDRAM_BASE6		0x60000000	/* base + 1536M  	*/
#define CFG_SDRAM_BASE7		0x70000000	/* base + 1792M  	*/

#define CFG_FLASH_BASE		0xfc000000	/* start of FLASH	*/
#define CFG_MONITOR_BASE	TEXT_BASE

#if 0
#define CFG_NAND_ADDR		0xd0000000      /* NAND Flash		*/
#endif

#define CFG_OCM_BASE		0xe0010000      /* ocm			*/

/* Don't change either of these */
#define CFG_PERIPHERAL_BASE	0xef600000	/* internal peripherals	*/

#define CFG_USB2D0_BASE		0xe0000100
#define CFG_USB_DEVICE		0xe0000000
#define CFG_USB_HOST		0xe0000400

/*-----------------------------------------------------------------------
 * Initial RAM & stack pointer
 *----------------------------------------------------------------------*/
/* 440EPx/440GRx have 16KB of internal SRAM, so no need for D-Cache	*/
#define CFG_INIT_RAM_OCM	1		/* OCM as init ram	*/
#define CFG_INIT_RAM_ADDR	CFG_OCM_BASE	/* OCM			*/

#define CFG_INIT_RAM_END	(4 << 10)
#define CFG_GBL_DATA_SIZE	256		/* num bytes initial data */
#define CFG_GBL_DATA_OFFSET	(CFG_INIT_RAM_END - CFG_GBL_DATA_SIZE)
#define CFG_INIT_SP_OFFSET	CFG_GBL_DATA_OFFSET

/*-----------------------------------------------------------------------
 * Serial Port
 *----------------------------------------------------------------------*/
#define CFG_EXT_SERIAL_CLOCK	11059200	/* ext. 11.059MHz clk	*/
#define CONFIG_BAUDRATE		115200
#define CONFIG_SERIAL_MULTI     1
/* define this if you want console on UART1 */
#undef CONFIG_UART1_CONSOLE

#define CFG_BAUDRATE_TABLE						\
	{300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200}

/*-----------------------------------------------------------------------
 * Environment
 *----------------------------------------------------------------------*/
#define CFG_ENV_IS_IN_FLASH     1	/* use FLASH for environment vars	*/

/*-----------------------------------------------------------------------
 * FLASH related
 *----------------------------------------------------------------------*/
#define CFG_FLASH_CFI				/* The flash is CFI compatible	*/
#define CFG_FLASH_CFI_DRIVER			/* Use common CFI driver	*/

#define CFG_FLASH_BANKS_LIST	{ CFG_FLASH_BASE }

#define CFG_MAX_FLASH_BANKS	1	/* max number of memory banks		*/
#define CFG_MAX_FLASH_SECT	512	/* max number of sectors on one chip	*/

#define CFG_FLASH_ERASE_TOUT	120000	/* Timeout for Flash Erase (in ms)	*/
#define CFG_FLASH_WRITE_TOUT	500	/* Timeout for Flash Write (in ms)	*/

#define CFG_FLASH_USE_BUFFER_WRITE 1	/* use buffered writes (20x faster)	*/
#define CFG_FLASH_PROTECTION	1	/* use hardware flash protection	*/

#define CFG_FLASH_EMPTY_INFO		/* print 'E' for empty sector on flinfo */
#define CFG_FLASH_QUIET_TEST	1	/* don't warn upon unknown flash	*/

#ifdef CFG_ENV_IS_IN_FLASH
#define CFG_ENV_SECT_SIZE	0x20000 	/* size of one complete sector	*/
#define CFG_ENV_ADDR		((-CFG_MONITOR_LEN)-CFG_ENV_SECT_SIZE)
#define	CFG_ENV_SIZE		0x2000	/* Total Size of Environment Sector	*/

/* Address and size of Redundant Environment Sector	*/
#define CFG_ENV_ADDR_REDUND	(CFG_ENV_ADDR-CFG_ENV_SECT_SIZE)
#define CFG_ENV_SIZE_REDUND	(CFG_ENV_SIZE)
#endif

/*
 * IPL (Initial Program Loader, integrated inside CPU)
 * Will load first 4k from NAND (SPL) into cache and execute it from there.
 *
 * SPL (Secondary Program Loader)
 * Will load special U-Boot version (NUB) from NAND and execute it. This SPL
 * has to fit into 4kByte. It sets up the CPU and configures the SDRAM
 * controller and the NAND controller so that the special U-Boot image can be
 * loaded from NAND to SDRAM.
 *
 * NUB (NAND U-Boot)
 * This NAND U-Boot (NUB) is a special U-Boot version which can be started
 * from RAM. Therefore it mustn't (re-)configure the SDRAM controller.
 *
 * On 440EPx the SPL is copied to SDRAM before the NAND controller is
 * set up. While still running from cache, I experienced problems accessing
 * the NAND controller.	sr - 2006-08-25
 */
#if 0
#define CFG_NAND_BOOT_SPL_SRC	0xfffff000	/* SPL location			*/
#define CFG_NAND_BOOT_SPL_SIZE	(4 << 10)	/* SPL size			*/
#define CFG_NAND_BOOT_SPL_DST	(CFG_OCM_BASE + (12 << 10)) /* Copy SPL here	*/
#define CFG_NAND_U_BOOT_DST	0x01000000	/* Load NUB to this addr	*/
#define CFG_NAND_U_BOOT_START	CFG_NAND_U_BOOT_DST /* Start NUB from this addr	*/
#define CFG_NAND_BOOT_SPL_DELTA	(CFG_NAND_BOOT_SPL_SRC - CFG_NAND_BOOT_SPL_DST)

/*
 * Define the partitioning of the NAND chip (only RAM U-Boot is needed here)
 */
#define CFG_NAND_U_BOOT_OFFS	(16 << 10)	/* Offset to RAM U-Boot image	*/
#define CFG_NAND_U_BOOT_SIZE	(384 << 10)	/* Size of RAM U-Boot image	*/

/*
 * Now the NAND chip has to be defined (no autodetection used!)
 */
#define CFG_NAND_PAGE_SIZE	(512)		/* NAND chip page size		*/
#define CFG_NAND_BLOCK_SIZE	(16 << 10)	/* NAND chip block size		*/
#define CFG_NAND_PAGE_COUNT	(32)		/* NAND chip page count		*/
#define CFG_NAND_BAD_BLOCK_POS	(5)		/* Location of bad block marker	*/
#undef CFG_NAND_4_ADDR_CYCLE			/* No fourth addr used (<=32MB)	*/
#endif

/*-----------------------------------------------------------------------
 * DDR SDRAM
 *----------------------------------------------------------------------*/
#define SPD_EEPROM_ADDRESS      0x50

#define CFG_DRAM_TEST           1
#define CONFIG_DDR_DATA_EYE 		/* use DDR2 optimization	*/

/*-----------------------------------------------------------------------
 * I2C
 *----------------------------------------------------------------------*/
#define CONFIG_HARD_I2C		1		/* I2C with hardware support	*/
#undef	CONFIG_SOFT_I2C				/* I2C bit-banged		*/
#define CFG_I2C_SPEED		400000		/* I2C speed and slave address	*/
#define CFG_I2C_SLAVE		0x7F

#define CFG_I2C_MULTI_EEPROMS
#define CFG_I2C_EEPROM_ADDR	(0xa8>>1)
#define CFG_I2C_EEPROM_ADDR_LEN 1
#define CFG_EEPROM_PAGE_WRITE_ENABLE
#define CFG_EEPROM_PAGE_WRITE_BITS 3
#define CFG_EEPROM_PAGE_WRITE_DELAY_MS 10

/* I2C SYSMON (LM75, AD7414 is almost compatible)			*/
#define CONFIG_DTT_LM75		1		/* ON Semi's LM75	*/
#define CONFIG_DTT_AD7414	1		/* use AD7414		*/
#define CONFIG_DTT_I2C_DEV_CODE 0x4d
#define CONFIG_DTT_SENSORS	{0}		/* Sensor addresses	*/
#define CFG_DTT_MAX_TEMP	70
#define CFG_DTT_LOW_TEMP	-30
#define CFG_DTT_HYSTERESIS	3

#define CONFIG_PREBOOT	        "echo;" \
  "echo Type \"run bit\" to run test routines;" \
  "echo Type \"run init_eeprom\" to program eeprom configuration H;" \
  "echo"

#undef	CONFIG_BOOTARGS

/* Setup some board specific values for the default environment variables */

/* allow people to clobber ethernet address */
#define CONFIG_ENV_OVERWRITE

#define CONFIG_HOSTNAME		roach   
#define CFG_BOOTFILE		"bootfile=/tftpboot/roach/uImage\0"
#define CFG_ROOTPATH		"rootpath=/opt/eldk/ppc_4xxFP\0"

#define	CONFIG_EXTRA_ENV_SETTINGS					\
	CFG_BOOTFILE							\
	CFG_ROOTPATH							\
	"netdev=eth0\0" \
	"ramargs=setenv bootargs root=/dev/ram rw\0" \
	"addip=setenv bootargs ${bootargs} " \
		"ip=${ipaddr}:${serverip}:${gatewayip}:${netmask}" \
		":${hostname}:${netdev}:off panic=1\0" \
	"addtty=setenv bootargs ${bootargs} console=ttyS0,${baudrate}\0" \
	"ethaddr=02:02:02:02:02:02\0" \
	"flash_self=run ramargs addip addtty;" \
		"bootm ${kernel_addr} ${ramdisk_addr}\0" \
	"init_eeprom=echo Programming EEPROM for configuration H;" \
	  "imw 0x52 0.1 87; imw 0x52 1.1 78; imw 0x52 2.1 82; imw 0x52 3.1 52;" \
		"imw 0x52 4.1 09; imw 0x52 5.1 57; imw 0x52 6.1 a0; imw 0x52 7.1 30;" \
		"imw 0x52 8.1 40; imw 0x52 9.1 08; imw 0x52 a.1 23; imw 0x52 b.1 50;" \
		"imw 0x52 c.1 0d; imw 0x52 d.1 05; imw 0x52 e.1 00; imw 0x52 f.1 00\0" \
	"bit=report clear;" \
		"check serial-output echo serial check;" \
		"check cpld-probe roachcpld;" \
		"mw.l 0x201000 0 400; check mem-zero cmp.l 0x201000 0x201800 200;" \
		"mw.l 0x200000 0xdeadbeef 200; mw.l 0x200800 0xff5aa500 100;" \
		"mw.l 0x200c00 0x3c00ffc3 100; cp.b 0x200000 0x201000 1000;" \
		"check mem-mem-cp cmp.b 0x200000 0x201000 1000;" \
		"cp.b 0xfc000000 0x200000 1000; check flash-mem-cmp cmp.b 0x200000 0xfc000000 1000;" \
		"check flash-erase erase 0xfc000000 +1000;" \
		"cp.b 0x200000 0xfc000000 1000; check flash-write cmp.b 0x200000 0xfc000000 1000;" \
		"usb start;" \
		"check usb-load fatload usb 0 0x200000 roach_bsp.bin;" \
		"check selectmap-program roachsmap 0x200000;" \
		"check mmc-presence mmctest;" \
		"check net-dhcp dhcp;" \
		"check fpga-dram-test roachddr;" \
		"check qdr-0-test roachqdr 0;" \
		"check qdr-1-test roachqdr 1;" \
		"check adc-0-test roachadc 0;" \
		"check adc-1-test roachadc 1;" \
		"check 10Gb-net-0-test roach10ge 0;" \
		"check 10Gb-net-1-test roach10ge 1;" \
		"check 10Gb-net-2-test roach10ge 2;" \
		"check 10Gb-net-3-test roach10ge 3;" \
		"echo Test Report;report\0" \
	"kernel_addr=FC000000\0" \
	"ramdisk_addr=FC180000\0" \
	"update=protect off FFFA0000 FFFFFFFF;era FFFA0000 FFFFFFFF;" \
		"cp.b 200000 FFFA0000 60000\0" \
	""
#define CONFIG_BOOTCOMMAND	"run flash_self"

#if 0
#define CONFIG_BOOTDELAY	5	/* autoboot after 5 seconds	*/
#else
#define CONFIG_BOOTDELAY	-1	/* autoboot disabled		*/
#endif

#define CONFIG_LOADS_ECHO	1	/* echo on for serial download	*/
#define CFG_LOADS_BAUD_CHANGE	1	/* allow baudrate change	*/

#if 0
#define CONFIG_M88E1111_PHY	1
#endif
#define CONFIG_DP83865_PHY      1       /* National DP83865 */
#define	CONFIG_IBM_EMAC4_V4	1
#define CONFIG_MII		1	/* MII PHY management		*/
#define CONFIG_PHY_ADDR	        0x1e	/* PHY address, See schematics	*/
#if 0
#define CONFIG_PHY_ADDR		0	/* PHY address, See schematics	*/
#endif


#define CONFIG_PHY_RESET        1	/* reset phy upon startup         */
#define CONFIG_PHY_GIGE		1	/* Include GbE speed/duplex detection */

#define CONFIG_HAS_ETH0
#define CFG_RX_ETH_BUFFER	32	/* Number of ethernet rx buffers & descriptors */

#define CONFIG_NET_MULTI	1
#if 0
#define CONFIG_HAS_ETH1		1	/* add support for "eth1addr"	*/
#define CONFIG_PHY1_ADDR	1
#endif
/* USB */
#ifdef CONFIG_440EPX
#define CONFIG_USB_OHCI
#define CONFIG_USB_STORAGE

/* Comment this out to enable USB 1.1 device */
#define USB_2_0_DEVICE

#define CMD_USB			CFG_CMD_USB
#else
#define CMD_USB			0	/* no USB on 440GRx		*/
#endif /* CONFIG_440EPX */

/* Partitions */
#define CONFIG_MAC_PARTITION
#define CONFIG_DOS_PARTITION
#define CONFIG_ISO_PARTITION

#define CONFIG_COMMANDS       (CONFIG_CMD_DFL	 |	\
			       CFG_CMD_ASKENV	 |	\
			       CFG_CMD_DHCP	 |	\
			       CFG_CMD_DTT	 |	\
			       CFG_CMD_DIAG	 |	\
			       CFG_CMD_EEPROM	 |	\
			       CFG_CMD_ELF	 |	\
			       CFG_CMD_FAT	 | 	\
			       CFG_CMD_I2C	 |	\
			       CFG_CMD_IRQ	 |	\
			       CFG_CMD_MII	 |	\
			       CFG_CMD_NET	 |	\
			       CFG_CMD_NFS	 |	\
			       CFG_CMD_PING	 |	\
			       CFG_CMD_REGINFO	 |	\
			       CFG_CMD_SDRAM	 |	\
                               CFG_CMD_ROACH     |      \
                               CFG_CMD_MMC       |      \
			       CMD_USB)

#define CONFIG_SUPPORT_VFAT

/* this must be included AFTER the definition of CONFIG_COMMANDS (if any) */
#include <cmd_confdefs.h>

/*-----------------------------------------------------------------------
 * Miscellaneous configurable options
 *----------------------------------------------------------------------*/
#define CFG_LONGHELP			/* undef to save memory		*/
#define CFG_PROMPT	        "=> "	/* Monitor Command Prompt	*/
#define CFG_CBSIZE	        1024	/* Console I/O Buffer Size	*/
#define CFG_PBSIZE              (CFG_CBSIZE+sizeof(CFG_PROMPT)+16) /* Print Buffer Size */
#define CFG_MAXARGS	        16	/* max number of command args	*/
#define CFG_BARGSIZE	        CFG_CBSIZE /* Boot Argument Buffer Size	*/

#define CFG_MEMTEST_START	0x0400000 /* memtest works on		*/
#define CFG_MEMTEST_END		0x0C00000 /* 4 ... 12 MB in DRAM	*/

#define CFG_LOAD_ADDR		0x100000  /* default load address	*/
#define CFG_EXTBDINFO		1	/* To use extended board_into (bd_t) */

#define CFG_HZ		        1000	/* decrementer freq: 1 ms ticks	*/

#define CONFIG_CMDLINE_EDITING	1	/* add command line history	*/
#define CONFIG_LOOPW            1       /* enable loopw command         */
#define CONFIG_MX_CYCLIC        1       /* enable mdc/mwc commands      */
#define CONFIG_ZERO_BOOTDELAY_CHECK	/* check for keypress on bootdelay==0 */
#define CONFIG_VERSION_VARIABLE 1	/* include version env variable */

/*
 * For booting Linux, the board info and command line data
 * have to be in the first 8 MB of memory, since this is
 * the maximum mapped by the Linux kernel during initialization.
 */
#define CFG_BOOTMAPSZ		(8 << 20)	/* Initial Memory map for Linux */

/*-----------------------------------------------------------------------
 * External Bus Controller (EBC) Setup
 *----------------------------------------------------------------------*/

/* supporting AP macros */
#define EBC_AP_BME     0x80000000
#define EBC_AP_TWT(p)  (((p) << 23) & 0x7f800000)
#define EBC_AP_CSN(p)  (((p) << 18) & 0x000c0000)
#define EBC_AP_OEN(p)  (((p) << 16) & 0x00030000)
#define EBC_AP_WBN(p)  (((p) << 14) & 0x0000c000)
#define EBC_AP_WBF(p)  (((p) << 12) & 0x00003000)
#define EBC_AP_TH(p)   (((p) <<  9) & 0x00000e00)
#define EBC_AP_RE      0x00000100
#define EBC_AP_SOR     0x00000080
#define EBC_AP_BEM     0x00000040
#define EBC_AP_PEN     0x00000020
/* supporting bank macros */
#define EBC_BANK_8B    0x00000000
#define EBC_BANK_16B   0x00002000
#define EBC_BANK_32B   0x00004000
#define EBC_BANK_READ  0x00008000
#define EBC_BANK_WRITE 0x00010000
#define EBC_BANK_1M    0x00000000
#define EBC_BANK_2M    0x00020000
#define EBC_BANK_4M    0x00040000
#define EBC_BANK_8M    0x00060000
#define EBC_BANK_16M   0x00080000
#define EBC_BANK_32M   0x000a0000
#define EBC_BANK_64M   0x000c0000
#define EBC_BANK_128M  0x000e0000

#define CFG_FLASH		CFG_FLASH_BASE

#define CFG_CPLD_BASE   0xC0000000
#define CFG_FPGA_BASE   0xD0000000
#define CFG_SMAP_BASE   0xC0100000

/* Bank 0 (NOR-FLASH) initialization */
#if 0
#define CFG_EBC_PB0AP		0x03017200
#define CFG_EBC_PB0CR		(CFG_FLASH | 0xda000)
#endif
#define CFG_EBC_PB0AP		(EBC_AP_TWT(13) | EBC_AP_CSN(0) | EBC_AP_OEN(2) | EBC_AP_WBN(1) | EBC_AP_WBF(3) | EBC_AP_TH(2))
#define CFG_EBC_PB0CR		(CFG_FLASH | EBC_BANK_64M | EBC_BANK_READ | EBC_BANK_WRITE | EBC_BANK_16B)

/* Bank 1 (FPGA) initialization	*/
#ifdef CFG_FPGA_BASE
#define CFG_EBC_PB1AP		(EBC_AP_TWT(0) | EBC_AP_CSN(1) | EBC_AP_OEN(0) | EBC_AP_WBN(0) | EBC_AP_WBF(0) | EBC_AP_TH(1) | EBC_AP_RE | EBC_AP_SOR)
#define CFG_EBC_PB1CR		(CFG_FPGA_BASE | EBC_BANK_128M | EBC_BANK_READ | EBC_BANK_WRITE | EBC_BANK_16B)
#endif

#ifdef CFG_CPLD_BASE
/* Bank 2 (CPLD) initialization	*/
#define CFG_EBC_PB2AP		(EBC_AP_TWT(0) | EBC_AP_CSN(0) | EBC_AP_OEN(0) | EBC_AP_WBN(0) | EBC_AP_WBF(0) | EBC_AP_TH(1) | EBC_AP_SOR)
#define CFG_EBC_PB2CR		(CFG_CPLD_BASE | EBC_BANK_1M | EBC_BANK_READ | EBC_BANK_WRITE | EBC_BANK_8B)
#endif

#if 0
/* Bank 3 (NAND-FLASH) initialization */
#define CFG_EBC_PB3AP		0x018003c0
#define CFG_EBC_PB3CR		(CFG_NAND | 0x1c000)
#endif

/* Bank 4 (selectmap) initialization */
#ifdef CFG_SMAP_BASE
#define CFG_EBC_PB4AP		(EBC_AP_TWT(0) | EBC_AP_CSN(0) | EBC_AP_OEN(0) | EBC_AP_WBN(0) | EBC_AP_WBF(0) | EBC_AP_TH(1) | EBC_AP_SOR)
#define CFG_EBC_PB4CR		(CFG_SMAP_BASE | EBC_BANK_1M  | EBC_BANK_READ | EBC_BANK_WRITE | EBC_BANK_16B)
#endif


/*-----------------------------------------------------------------------
 * NAND FLASH
 *----------------------------------------------------------------------*/
#if 0
#define CFG_MAX_NAND_DEVICE	1
#define NAND_MAX_CHIPS		1
#define CFG_NAND_BASE		(CFG_NAND_ADDR + CFG_NAND_CS)
#define CFG_NAND_SELECT_DEVICE  1	/* nand driver supports mutipl. chips	*/
#endif

/*-----------------------------------------------------------------------
 * Cache Configuration
 *----------------------------------------------------------------------*/
#define CFG_DCACHE_SIZE		(32<<10)  /* For AMCC 440 CPUs			*/
#define CFG_CACHELINE_SIZE	32	      /* ...			            */
#if (CONFIG_COMMANDS & CFG_CMD_KGDB)
#define CFG_CACHELINE_SHIFT	5	      /* log base 2 of the above value	*/
#endif

/*
 * Internal Definitions
 *
 * Boot Flags
 */
#define BOOTFLAG_COLD	0x01		/* Normal Power-On: Boot from FLASH	*/
#define BOOTFLAG_WARM	0x02		/* Software reboot			*/

#if (CONFIG_COMMANDS & CFG_CMD_KGDB)
#define CONFIG_KGDB_BAUDRATE	230400	/* speed to run kgdb serial port */
#define CONFIG_KGDB_SER_INDEX	2	    /* which serial port to use */
#endif
#endif	/* __CONFIG_H */
