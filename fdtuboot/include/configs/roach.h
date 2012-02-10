/*
 * (C) Copyright 2006-2008
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

/*
 * roach.h - configuration for Roach board
 */
#ifndef __CONFIG_H
#define __CONFIG_H

/*
 * High Level Configuration Options
 */
#define CONFIG_CMD_DTT        1
#define CONFIG_ROACH     	    1		/* Board is Roach	*/

#define CONFIG_CMD_ROACHTEST  1	
#define CONFIG_CMD_RMON       1	
#define CONFIG_CMD_SMAP     	1
#define CONFIG_CMD_CHECK      1

#define CFG_MAXARGS         16  /* max number of command args   */
#define CFG_BARGSIZE            CFG_CBSIZE /* Boot Argument Buffer Size */

#define CFG_MEMTEST_START   0x0400000 /* memtest works on       */
#define CFG_MEMTEST_END     0x0C00000 /* 4 ... 12 MB in DRAM    */

#define CFG_LOAD_ADDR       0x100000  /* default load address   */
#define CFG_EXTBDINFO       1    


#define ROACH_V1_01
#define CONFIG_440EPX		1		/* Specific PPC440EPx	*/
#define CONFIG_HOSTNAME		roach
#define CONFIG_440		1	/* ... PPC440 family		*/
#define CONFIG_4xx		1	/* ... PPC4xx family		*/

#define CONFIG_BOARD_EARLY_INIT_F 1	/* Call board_early_init_f	*/
#define CONFIG_BOARD_EARLY_INIT_R 1	/* Call board_early_init_r	*/
#define CONFIG_MISC_INIT_R	1	/* Call misc_init_r		*/

#define CONFIG_SYS_SDRAM_BASE		0x00000000	/* _must_ be 0		*/
#define CONFIG_SYS_MONITOR_BASE	TEXT_BASE	/* Start of U-Boot	*/
#define CONFIG_SYS_MONITOR_LEN		(0xFFFFFFFF - CONFIG_SYS_MONITOR_BASE + 1)
#define CONFIG_SYS_MALLOC_LEN		(1 << 20)	/* Reserved for malloc	*/

/*
 * UART
 */
#define CONFIG_BAUDRATE		115200
#define CONFIG_SERIAL_MULTI
#define CONFIG_SYS_BAUDRATE_TABLE  \
    {300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400}

/*
 * I2C
 */
#define CONFIG_HARD_I2C			/* I2C with hardware support	*/
#define CONFIG_SYS_I2C_SLAVE		0x7F

/*
 * Ethernet/EMAC/PHY
 */
#define CONFIG_PPC4xx_EMAC
#define CONFIG_MII			/* MII PHY management		*/
#define CONFIG_NET_MULTI
#define CONFIG_NETCONSOLE		/* include NetConsole support	*/
#if defined(CONFIG_440)
#define CONFIG_SYS_RX_ETH_BUFFER	32	/* number of eth rx buffers	*/
#else
#define CONFIG_SYS_RX_ETH_BUFFER	16	/* number of eth rx buffers	*/
#endif

#define CONFIG_ENV_OVERWRITE

/*
 * Commands
 */
#include <config_cmd_default.h>

#define CONFIG_CMD_ASKENV
#if defined(CONFIG_440)
#define CONFIG_CMD_CACHE
#endif
#define CONFIG_CMD_DHCP
#define CONFIG_CMD_DIAG
#define CONFIG_CMD_EEPROM
#define CONFIG_CMD_ELF
#define CONFIG_CMD_I2C
#define CONFIG_CMD_IRQ
#define CONFIG_CMD_MII
#define CONFIG_CMD_NET
#define CONFIG_CMD_NFS
#define CONFIG_CMD_PING
#define CONFIG_CMD_REGINFO

/*
 * Miscellaneous configurable options
 */
#define CONFIG_SYS_LONGHELP			/* undef to save memory		*/
#define CONFIG_SYS_PROMPT		"=> "	/* Monitor Command Prompt	*/
#if defined(CONFIG_CMD_KGDB)
#define CONFIG_SYS_CBSIZE		1024	/* Console I/O Buffer Size	*/
#else
#define CONFIG_SYS_CBSIZE		256	/* Console I/O Buffer Size	*/
#endif
#define CONFIG_SYS_PBSIZE		(CONFIG_SYS_CBSIZE+sizeof(CONFIG_SYS_PROMPT)+16)
#define CONFIG_SYS_MAXARGS		16	/* max number of command args	*/
#define CONFIG_SYS_BARGSIZE		CONFIG_SYS_CBSIZE /* Boot Argument Buffer Size	*/

#define CONFIG_SYS_MEMTEST_START	0x0400000 /* memtest works on		*/
#define CONFIG_SYS_MEMTEST_END		0x0C00000 /* 4 ... 12 MB in DRAM	*/

#define CONFIG_SYS_LOAD_ADDR		0x100000  /* default load address	*/
#define CONFIG_SYS_EXTBDINFO			/* To use extended board_into (bd_t) */

#define CONFIG_SYS_HZ			1000	/* decrementer freq: 1 ms ticks	*/

#define CONFIG_CMDLINE_EDITING		/* add command line history	*/
#define CONFIG_AUTO_COMPLETE		/* add autocompletion support	*/
#define CONFIG_LOOPW			/* enable loopw command         */
#define CONFIG_MX_CYCLIC		/* enable mdc/mwc commands      */
#define CONFIG_ZERO_BOOTDELAY_CHECK	/* check for keypress on bootdelay==0 */
#define CONFIG_VERSION_VARIABLE 	/* include version env variable */
#define CONFIG_SYS_CONSOLE_INFO_QUIET		/* don't print console @ startup*/

#define CONFIG_SYS_HUSH_PARSER			/* Use the HUSH parser		*/
#ifdef	CONFIG_SYS_HUSH_PARSER
#define	CONFIG_SYS_PROMPT_HUSH_PS2	"> "
#endif

#define CONFIG_LOADS_ECHO		/* echo on for serial download	*/
#define CONFIG_SYS_LOADS_BAUD_CHANGE		/* allow baudrate change	*/

/*
 * BOOTP options
 */
#define CONFIG_BOOTP_BOOTFILESIZE
#define CONFIG_BOOTP_BOOTPATH
#define CONFIG_BOOTP_GATEWAY
#define CONFIG_BOOTP_HOSTNAME
#define CONFIG_BOOTP_SUBNETMASK

/*
 * For booting Linux, the board info and command line data
 * have to be in the first 8 MB of memory, since this is
 * the maximum mapped by the Linux kernel during initialization.
 */
#define CONFIG_SYS_BOOTMAPSZ		(8 << 20) /* Initial Memory map for Linux */

/*
 * Internal Definitions
 */
#if defined(CONFIG_CMD_KGDB)
#define CONFIG_KGDB_BAUDRATE	230400	/* speed to run kgdb serial port*/
#define CONFIG_KGDB_SER_INDEX	2	/* which serial port to use	*/
#endif

/*
 * Pass open firmware flat tree
 */
#define CONFIG_OF_LIBFDT
#define CONFIG_OF_BOARD_SETUP

/*
 * Booting and default environment
 */
#define CONFIG_PREBOOT	    "run initboot"
#define CONFIG_BOOTCOMMAND	"bootm FC000000"

#define CONFIG_BOOTDELAY	-1	/* autoboot disabled		*/

/*
 * Only very few boards have default console not on ttyS0 (like Taishan)
 */
#if !defined(CONFIG_USE_TTY)
#define CONFIG_USE_TTY	ttyS0
#endif

/*
 * Only very few boards have default netdev not set to eth0 (like Arches)
 */
#if !defined(CONFIG_USE_NETDEV)
#define CONFIG_USE_NETDEV	eth0
#endif

/*
 * Only some 4xx PPC's are equipped with an FPU
 */
#if defined(CONFIG_440EP) || defined(CONFIG_440EPX) || \
    defined(CONFIG_460EX) || defined(CONFIG_460GT)
#define CONFIG_AMCC_DEF_ENV_ROOTPATH	"rootpath=/opt/eldk/ppc_4xxFP\0"
#else
#define CONFIG_AMCC_DEF_ENV_ROOTPATH	"rootpath=/opt/eldk/ppc_4xx\0"
#endif

#define xstr(s)	str(s)
#define str(s)	#s

/*
 * Default environment variables
 */
#define	CONFIG_EXTRA_ENV_SETTINGS					\
	"ethaddr=02:02:02:02:02:02\0" \
	"netdev=eth0\0" \
	"init_eeprom=echo Programming EEPROM for configuration H;" \
	  "imw 0x52 0.1 87; imw 0x52 1.1 78; imw 0x52 2.1 82; imw 0x52 3.1 52;" \
		"imw 0x52 4.1 09; imw 0x52 5.1 57; imw 0x52 6.1 a0; imw 0x52 7.1 30;" \
		"imw 0x52 8.1 40; imw 0x52 9.1 08; imw 0x52 a.1 23; imw 0x52 b.1 50;" \
		"imw 0x52 c.1 0d; imw 0x52 d.1 05; imw 0x52 e.1 00; imw 0x52 f.1 00;" \
    "rmon w 0xffff 2\0" \
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
	"kernel_addr=FC000000\0"  \
  "fdt_addr=FC1C0000\0" \
	"boot=bootm ${kernel_addr} - ${fdt_addr}\0" \
	"bootargs=console=ttyS0,115200 mtdparts=physmap-flash.0:1792k(linux),256k@0x1c0000(fdt),4096k@0x200000(root),58752k@0x600000(usr),256k@0x3f60000(env),384k@0x3fa0000(uboot) root=/dev/mtdblock2\0" \
	"yget=loady 200000\0" \
	"update=protect off " xstr(CONFIG_SYS_MONITOR_BASE) " FFFFFFFF;"	\
	"era " xstr(CONFIG_SYS_MONITOR_BASE) " FFFFFFFF;"	\
		"cp.b 200000 " xstr(CONFIG_SYS_MONITOR_BASE) " ${filesize};" \
    "protect on " xstr(CONFIG_SYS_MONITOR_BASE) " FFFFFFFF\0" \
	"newuboot=run yget update\0"	\
	"newkernel=run yget; era 0xfc000000 0xfc1bffff; cp.b 0x200000 0xfc000000 0x1c0000\0" \
  "initboot=run firstime\0" \
  "firstime=run init_eeprom bit; setenv initboot run nexttime; setenv ethaddr; saveenv\0" \
  "nexttime=echo; echo type \"run bit\" to run tests; echo\0" \
  "nfs=setenv bootargs console=ttyS0,115200 mtdparts=physmap-flash.0:1792k(linux),256k@0x1c0000(fdt),4096k@0x200000(root),58752k@0x600000(usr),256k@0x3f60000(env),384k@0x3fa0000(uboot) root=/dev/nfs nfsroot=192.168.1.48:/home/nfs/etch ip=dhcp init=/bin/bash\0" \
  "nexttime=echo; echo type \"run bit\" to run tests; echo\0" \
	""

/* Hardcoded clock frequency */
#define CONFIG_SYS_CLK_FREQ         33000000

/*
 * Base addresses -- Note these are effective addresses where the actual
 * resources get mapped (not physical addresses).
 */

/* CPU can't see more than 2G */
#define CONFIG_SYS_SDRAM_BASE			0x00000000	/* _must_ be 0		*/
#define CONFIG_SYS_SDRAM_BASE1		0x10000000	/* base +  256M  	*/
#define CONFIG_SYS_SDRAM_BASE2		0x20000000	/* base +  512M  	*/
#define CONFIG_SYS_SDRAM_BASE3		0x30000000	/* base +  768M  	*/
#define CONFIG_SYS_SDRAM_BASE4		0x40000000	/* base + 1024M  	*/
#define CONFIG_SYS_SDRAM_BASE5		0x50000000	/* base + 1280M  	*/
#define CONFIG_SYS_SDRAM_BASE6		0x60000000	/* base + 1536M  	*/
#define CONFIG_SYS_SDRAM_BASE7		0x70000000	/* base + 1792M  	*/

#define CONFIG_SYS_MONITOR_BASE		TEXT_BASE

#define CONFIG_SYS_BOOT_BASE_ADDR	0xf0000000
#define CONFIG_SYS_FLASH_BASE	  	0xfc000000	/* start of FLASH	*/

#define CONFIG_SYS_OCM_BASE		    0xe0010000	/* ocm			*/
#define CONFIG_SYS_OCM_DATA_ADDR	CONFIG_SYS_OCM_BASE

/* Don't change either of these */
#define CONFIG_SYS_PERIPHERAL_BASE	0xef600000	/* internal peripherals	*/

#define CONFIG_SYS_USB2D0_BASE	 	0xe0000100
#define CONFIG_SYS_USB_DEVICE	   	0xe0000000
#define CONFIG_SYS_USB_HOST		    0xe0000400

/*
 * Initial RAM & stack pointer
 */
/* 440EPx/440GRx have 16KB of internal SRAM, so no need for D-Cache	*/
#define CONFIG_SYS_INIT_RAM_ADDR	CONFIG_SYS_OCM_BASE	/* OCM			*/
#define CONFIG_SYS_INIT_RAM_END	(4 << 10)
#define CONFIG_SYS_GBL_DATA_SIZE	256	/* num bytes initial data	*/
#define CONFIG_SYS_GBL_DATA_OFFSET	(CONFIG_SYS_INIT_RAM_END - CONFIG_SYS_GBL_DATA_SIZE)
#define CONFIG_SYS_INIT_SP_OFFSET	CONFIG_SYS_POST_WORD_ADDR

/*
 * Serial Port
 */
#define CONFIG_SYS_EXT_SERIAL_CLOCK	11059200	/* ext. 11.059MHz clk	*/

/* define this if you want console on UART1 */
#undef CONFIG_UART1_CONSOLE

/*
 * Environment
 */
#define CONFIG_ENV_IS_IN_FLASH	1	/* use FLASH for environ vars	*/

/*
 * FLASH related
 */
#define CONFIG_SYS_FLASH_CFI			/* The flash is CFI compatible	*/
#define CONFIG_FLASH_CFI_DRIVER		/* Use common CFI driver	*/

#define CONFIG_SYS_FLASH_BANKS_LIST	{ CONFIG_SYS_FLASH_BASE }

#define CONFIG_SYS_MAX_FLASH_BANKS	1	/* max number of memory banks	      */
#define CONFIG_SYS_MAX_FLASH_SECT	512	/* max number of sectors on one chip  */

#define CONFIG_SYS_FLASH_ERASE_TOUT	120000	/* Timeout for Flash Erase (in ms)    */
#define CONFIG_SYS_FLASH_WRITE_TOUT	500	/* Timeout for Flash Write (in ms)    */

#define CONFIG_SYS_FLASH_USE_BUFFER_WRITE 1	/* use buffered writes (20x faster)   */
#define CONFIG_SYS_FLASH_PROTECTION	1	/* use hardware flash protection      */

#define CONFIG_SYS_FLASH_EMPTY_INFO	      /* print 'E' for empty sector on flinfo */
#define CONFIG_SYS_FLASH_QUIET_TEST	1	/* don't warn upon unknown flash      */

#ifdef CONFIG_ENV_IS_IN_FLASH
#define CONFIG_ENV_SECT_SIZE	0x20000	/* size of one complete sector	      */
#define CONFIG_ENV_ADDR		((-CONFIG_SYS_MONITOR_LEN)-CONFIG_ENV_SECT_SIZE)
#define	CONFIG_ENV_SIZE		0x2000	/* Total Size of Environment Sector   */

/* Address and size of Redundant Environment Sector	*/
#define CONFIG_ENV_ADDR_REDUND	(CONFIG_ENV_ADDR-CONFIG_ENV_SECT_SIZE)
#define CONFIG_ENV_SIZE_REDUND	(CONFIG_ENV_SIZE)
#endif

/*
 * DDR SDRAM
 */
#define CONFIG_SYS_MBYTES_SDRAM        (2048)	/* 2048MB			*/
#if 0
#define CONFIG_SYS_MBYTES_SDRAM        (256)	/* single page for testing */
#endif
#define CONFIG_DDR_DATA_EYE		/* use DDR2 optimization	*/
#define CONFIG_SYS_MEM_TOP_HIDE	(4 << 10) /* don't use last 4kbytes	*/
					/* 440EPx errata CHIP 11	*/
#define SPD_EEPROM_ADDRESS      0x50

#define CFG_DRAM_TEST           1
/*
 * I2C
 */
#define CONFIG_SYS_I2C_SPEED		400000	/* I2C speed and slave address	*/

#define CONFIG_SYS_I2C_MULTI_EEPROMS
#define CONFIG_SYS_I2C_EEPROM_ADDR	(0xa8>>1)
#define CONFIG_SYS_I2C_EEPROM_ADDR_LEN 1
#define CONFIG_SYS_EEPROM_PAGE_WRITE_BITS 3
#define CONFIG_SYS_EEPROM_PAGE_WRITE_DELAY_MS 10

#define CONFIG_SYS_I2C_DTT_ADDR	0x4d
/* I2C SYSMON (LM75, AD7414 is almost compatible)			*/
#if defined(CONFIG_CMD_DTT)
#define CONFIG_DTT_LM75		1	/* ON Semi's LM75		*/
#define CONFIG_DTT_AD7414	1	/* use AD7414			*/
#define CONFIG_DTT_SENSORS	{0}	/* Sensor addresses		*/
#define CONFIG_SYS_DTT_MAX_TEMP	70
#define CONFIG_SYS_DTT_LOW_TEMP	-30
#define CONFIG_SYS_DTT_HYSTERESIS	3
#endif


#define	CONFIG_IBM_EMAC4_V4	1

#define CONFIG_PHY_RESET        1	/* reset phy upon startup	*/
#define CONFIG_PHY_GIGE		1	/* Include GbE speed/duplex detection */
#define CONFIG_PHY_ADDR	        0x1e

#define CONFIG_HAS_ETH0

/* USB */
#ifdef CONFIG_440EPX
#define CONFIG_USB_OHCI_NEW
#define CONFIG_USB_STORAGE
#define CONFIG_SYS_OHCI_BE_CONTROLLER


#undef CONFIG_SYS_USB_OHCI_BOARD_INIT
#define CONFIG_SYS_USB_OHCI_CPU_INIT	1
#define CONFIG_SYS_USB_OHCI_REGS_BASE	CONFIG_SYS_USB_HOST
#define CONFIG_SYS_USB_OHCI_SLOT_NAME	"ppc440"
#define CONFIG_SYS_USB_OHCI_MAX_ROOT_PORTS 15

/* Comment this out to enable USB 1.1 device */
#define USB_2_0_DEVICE

#endif /* CONFIG_440EPX */

/* Partitions */
#define CONFIG_MAC_PARTITION
#define CONFIG_DOS_PARTITION
#define CONFIG_ISO_PARTITION

/*
 * Commands additional to the ones defined in amcc-common.h
 */
#define CONFIG_CMD_FAT
#define CONFIG_CMD_SDRAM



#ifdef CONFIG_440EPX
#define CONFIG_CMD_USB
#endif

#ifndef CONFIG_RAINIER
#define CONFIG_SYS_POST_FPU_ON		CONFIG_SYS_POST_FPU
#else
#define CONFIG_SYS_POST_FPU_ON		0
#endif

/* POST support */

/* NOTE:Disabled POST cache support(CONFIG_SYS_POST_CACHE) 
 * which inturn disables a whole bunch
 * of tests associated with that,and deleted the free virtual address
 * line(CONFIG_SYS_POST_CACHE_ADDR) valued at 0x7fff0000*/

#define CONFIG_POST		( CONFIG_SYS_POST_CPU	   | \
		CONFIG_SYS_POST_ETHER	   | \
		CONFIG_SYS_POST_FPU_ON   | \
		CONFIG_SYS_POST_I2C	   | \
		CONFIG_SYS_POST_MEMORY   | \
		CONFIG_SYS_POST_SPR	   | \
		CONFIG_SYS_POST_UART)

#define CONFIG_SYS_POST_WORD_ADDR	(CONFIG_SYS_GBL_DATA_OFFSET - 0x4)
#define CONFIG_LOGBUFFER

#define CONFIG_SYS_CONSOLE_IS_IN_ENV	/* Otherwise it catches logbuffer as output */

#define CONFIG_SUPPORT_VFAT


/*
 * External Bus Controller (EBC) Setup
 */

/*
 * On Sequoia CS0 and CS3 are switched when configuring for NAND booting
 */

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

#define CONFIG_SYS_FLASH		CONFIG_SYS_FLASH_BASE

#define CONFIG_SYS_CPLD_BASE   0xC0000000
#define CONFIG_SYS_FPGA_BASE   0xD0000000
#define CONFIG_SYS_SMAP_BASE   0xC0100000

/* Memory Bank 0 (NOR-FLASH) initialization				*/
#define CONFIG_SYS_EBC_PB0AP		(EBC_AP_TWT(13) | EBC_AP_CSN(0) | EBC_AP_OEN(2) | EBC_AP_WBN(1) | EBC_AP_WBF(3) | EBC_AP_TH(2))
#if 0
#define CONFIG_SYS_EBC_PB0AP		(EBC_AP_TWT(13) | EBC_AP_CSN(0) | EBC_AP_OEN(1) | EBC_AP_WBN(1) | EBC_AP_WBF(3) | EBC_AP_TH(1))
#endif
#define CONFIG_SYS_EBC_PB0CR		(CONFIG_SYS_FLASH | EBC_BANK_64M | EBC_BANK_READ | EBC_BANK_WRITE | EBC_BANK_16B)

/* Bank 1 (FPGA) initialization	*/
#ifdef CONFIG_SYS_FPGA_BASE
#define CONFIG_SYS_EBC_PB1AP		(EBC_AP_TWT(0) | EBC_AP_CSN(1) | EBC_AP_OEN(0) | EBC_AP_WBN(0) | EBC_AP_WBF(0) | EBC_AP_TH(1) | EBC_AP_RE | EBC_AP_SOR)
#define CONFIG_SYS_EBC_PB1CR		(CONFIG_SYS_FPGA_BASE | EBC_BANK_128M | EBC_BANK_READ | EBC_BANK_WRITE | EBC_BANK_16B)
#endif

#ifdef CONFIG_SYS_CPLD_BASE
/* Bank 2 (CPLD) initialization	*/
#define CONFIG_SYS_EBC_PB2AP		(EBC_AP_TWT(0) | EBC_AP_CSN(0) | EBC_AP_OEN(0) | EBC_AP_WBN(0) | EBC_AP_WBF(0) | EBC_AP_TH(1) | EBC_AP_SOR)
#define CONFIG_SYS_EBC_PB2CR		(CONFIG_SYS_CPLD_BASE | EBC_BANK_1M | EBC_BANK_READ | EBC_BANK_WRITE | EBC_BANK_8B)
#endif

/* Bank 4 (selectmap) initialization */
#ifdef CONFIG_SYS_SMAP_BASE
#define CONFIG_SYS_EBC_PB4AP		(EBC_AP_TWT(0) | EBC_AP_CSN(0) | EBC_AP_OEN(0) | EBC_AP_WBN(0) | EBC_AP_WBF(0) | EBC_AP_TH(1) | EBC_AP_SOR)
#define CONFIG_SYS_EBC_PB4CR		(CONFIG_SYS_SMAP_BASE | EBC_BANK_1M  | EBC_BANK_READ | EBC_BANK_WRITE | EBC_BANK_16B)
#endif

/*
 * PPC440 GPIO Configuration
 */
/* test-only: take GPIO init from pcs440ep ???? in config file */
#define CONFIG_SYS_4xx_GPIO_TABLE { /*	  Out		  GPIO	Alternate1	Alternate2	Alternate3 */ \
{											\
/* GPIO Core 0 */									\
{GPIO0_BASE, GPIO_BI , GPIO_ALT1, GPIO_OUT_0}, /* GPIO0	EBC_ADDR(7)	DMA_REQ(2)	*/	\
{GPIO0_BASE, GPIO_BI , GPIO_ALT1, GPIO_OUT_0}, /* GPIO1	EBC_ADDR(6)	DMA_ACK(2)	*/	\
{GPIO0_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO2	EBC_ADDR(5)	DMA_EOT/TC(2)	*/	\
{GPIO0_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO3	EBC_ADDR(4)	DMA_REQ(3)	*/	\
{GPIO0_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO4	EBC_ADDR(3)	DMA_ACK(3)	*/	\
{GPIO0_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO5	EBC_ADDR(2)	DMA_EOT/TC(3)	*/	\
{GPIO0_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO6	EBC_CS_N(1)			*/	\
{GPIO0_BASE, GPIO_OUT, GPIO_ALT1, GPIO_OUT_0}, /* GPIO7	EBC_CS_N(2)			*/	\
{GPIO0_BASE, GPIO_OUT, GPIO_ALT1, GPIO_OUT_0}, /* GPIO8	EBC_CS_N(3)			*/	\
{GPIO0_BASE, GPIO_OUT, GPIO_ALT1, GPIO_OUT_0}, /* GPIO9	EBC_CS_N(4)			*/	\
{GPIO0_BASE, GPIO_OUT, GPIO_ALT1, GPIO_OUT_0}, /* GPIO10 EBC_CS_N(5)			*/	\
{GPIO0_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO11 EBC_BUS_ERR			*/	\
{GPIO0_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO12				*/	\
{GPIO0_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO13				*/	\
{GPIO0_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO14				*/	\
{GPIO0_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO15				*/	\
{GPIO0_BASE, GPIO_OUT, GPIO_ALT1, GPIO_OUT_1}, /* GPIO16 GMCTxD(4)			*/	\
{GPIO0_BASE, GPIO_OUT, GPIO_ALT1, GPIO_OUT_1}, /* GPIO17 GMCTxD(5)			*/	\
{GPIO0_BASE, GPIO_OUT, GPIO_ALT1, GPIO_OUT_1}, /* GPIO18 GMCTxD(6)			*/	\
{GPIO0_BASE, GPIO_OUT, GPIO_ALT1, GPIO_OUT_1}, /* GPIO19 GMCTxD(7)			*/	\
{GPIO0_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO20 RejectPkt0			*/	\
{GPIO0_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO21 RejectPkt1			*/	\
{GPIO0_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO22				*/	\
{GPIO0_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO23 SCPD0				*/	\
{GPIO0_BASE, GPIO_OUT, GPIO_ALT1, GPIO_OUT_1}, /* GPIO24 GMCTxD(2)			*/	\
{GPIO0_BASE, GPIO_OUT, GPIO_ALT1, GPIO_OUT_1}, /* GPIO25 GMCTxD(3)			*/	\
{GPIO0_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO26				*/	\
{GPIO0_BASE, GPIO_IN , GPIO_ALT2, GPIO_OUT_0}, /* GPIO27 EXT_EBC_REQ	USB2D_RXERROR	*/	\
{GPIO0_BASE, GPIO_OUT, GPIO_ALT2, GPIO_OUT_1}, /* GPIO28		USB2D_TXVALID	*/	\
{GPIO0_BASE, GPIO_OUT, GPIO_ALT2, GPIO_OUT_1}, /* GPIO29 EBC_EXT_HDLA	USB2D_PAD_SUSPNDM */	\
{GPIO0_BASE, GPIO_OUT, GPIO_ALT2, GPIO_OUT_1}, /* GPIO30 EBC_EXT_ACK	USB2D_XCVRSELECT*/	\
{GPIO0_BASE, GPIO_OUT, GPIO_ALT2, GPIO_OUT_1}, /* GPIO31 EBC_EXR_BUSREQ	USB2D_TERMSELECT*/	\
},											\
{											\
/* GPIO Core 1 */									\
{GPIO1_BASE, GPIO_OUT, GPIO_ALT1, GPIO_OUT_1}, /* GPIO32 USB2D_OPMODE0	EBC_DATA(2)	*/	\
{GPIO1_BASE, GPIO_OUT, GPIO_ALT1, GPIO_OUT_1}, /* GPIO33 USB2D_OPMODE1	EBC_DATA(3)	*/	\
{GPIO1_BASE, GPIO_IN , GPIO_ALT2, GPIO_OUT_0}, /* GPIO34 UART0_8PIN_DCD_N UART1_DSR_CTS_N UART2_SOUT*/ \
{GPIO1_BASE, GPIO_OUT, GPIO_ALT2, GPIO_OUT_1}, /* GPIO35 UART0_8PIN_DSR_N UART1_RTS_DTR_N UART2_SIN*/ \
{GPIO1_BASE, GPIO_IN , GPIO_ALT1, GPIO_OUT_0}, /* GPIO36 UART0_CTS_N	EBC_DATA(0)	UART3_SIN*/ \
{GPIO1_BASE, GPIO_OUT, GPIO_ALT1, GPIO_OUT_1}, /* GPIO37 UART0_RTS_N	EBC_DATA(1)	UART3_SOUT*/ \
{GPIO1_BASE, GPIO_OUT, GPIO_ALT2, GPIO_OUT_1}, /* GPIO38 UART0_8PIN_DTR_N UART1_SOUT	*/	\
{GPIO1_BASE, GPIO_IN , GPIO_ALT2, GPIO_OUT_0}, /* GPIO39 UART0_8PIN_RI_N UART1_SIN	*/	\
{GPIO1_BASE, GPIO_IN , GPIO_ALT1, GPIO_OUT_0}, /* GPIO40 UIC_IRQ(0)			*/	\
{GPIO1_BASE, GPIO_IN , GPIO_ALT1, GPIO_OUT_0}, /* GPIO41 UIC_IRQ(1)			*/	\
{GPIO1_BASE, GPIO_IN , GPIO_ALT1, GPIO_OUT_0}, /* GPIO42 UIC_IRQ(2)			*/	\
{GPIO1_BASE, GPIO_IN , GPIO_ALT1, GPIO_OUT_0}, /* GPIO43 UIC_IRQ(3)			*/	\
{GPIO1_BASE, GPIO_IN , GPIO_ALT1, GPIO_OUT_0}, /* GPIO44 UIC_IRQ(4)	DMA_ACK(1)	*/	\
{GPIO1_BASE, GPIO_IN , GPIO_ALT1, GPIO_OUT_0}, /* GPIO45 UIC_IRQ(6)	DMA_EOT/TC(1)	*/	\
{GPIO1_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO46 UIC_IRQ(7)	DMA_REQ(0)	*/	\
{GPIO1_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO47 UIC_IRQ(8)	DMA_ACK(0)	*/	\
{GPIO1_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO48 UIC_IRQ(9)	DMA_EOT/TC(0)	*/	\
{GPIO1_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO49  Unselect via TraceSelect Bit	*/	\
{GPIO1_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO50  Unselect via TraceSelect Bit	*/	\
{GPIO1_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO51  Unselect via TraceSelect Bit	*/	\
{GPIO1_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO52  Unselect via TraceSelect Bit	*/	\
{GPIO1_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO53  Unselect via TraceSelect Bit	*/	\
{GPIO1_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO54  Unselect via TraceSelect Bit	*/	\
{GPIO1_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO55  Unselect via TraceSelect Bit	*/	\
{GPIO1_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO56  Unselect via TraceSelect Bit	*/	\
{GPIO1_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO57  Unselect via TraceSelect Bit	*/	\
{GPIO1_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO58  Unselect via TraceSelect Bit	*/	\
{GPIO1_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO59  Unselect via TraceSelect Bit	*/	\
{GPIO1_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO60  Unselect via TraceSelect Bit	*/	\
{GPIO1_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO61  Unselect via TraceSelect Bit	*/	\
{GPIO1_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO62  Unselect via TraceSelect Bit	*/	\
{GPIO1_BASE, GPIO_IN , GPIO_SEL , GPIO_OUT_0}, /* GPIO63  Unselect via TraceSelect Bit	*/	\
}											\
}

#endif /* __CONFIG_H */
