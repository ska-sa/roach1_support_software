/* rinit setup (setup roach control processor: AMCC PPC440EPX) */
/* Marc Welz <marc@ska.ac.za> */

#include "ppcstuff.h"
#include "serial.h"

/*******************************************************************/

#define IO_TLB_EA      0x70000000
#define IO_TLB_RPN     0xef600000

#define IO_TLB0        (IO_TLB_EA | TLB_VALID | TLB_SIZE_1MB)
#define IO_TLB1        ((IO_TLB_RPN & 0xfffffc00) | (0x1))
#define IO_TLB2        (TLB_FLAG_WRITETHROUGH | TLB_FLAG_UNCACHED | TLB_FLAG_GUARDED | TLB_FLAG_USERREAD | TLB_FLAG_USERWRITE | TLB_FLAG_SUPREAD | TLB_FLAG_SUPWRITE)

/*******************************************************************/

#define SRAM_TLB_EA    0x71000000
#define SRAM_TLB_RPN   0xe0010000
#define SRAM_SIZE      0x00004000

#define SRAM_TLB0      (SRAM_TLB_EA | TLB_VALID | TLB_SIZE_16KB)
#define SRAM_TLB1      ((SRAM_TLB_RPN & 0xfffffc00) | (0x0))
#define SRAM_TLB2      (TLB_FLAG_UNCACHED | TLB_FLAG_GUARDED | TLB_FLAG_USERREAD | TLB_FLAG_USERWRITE | TLB_FLAG_USEREXECUTE | TLB_FLAG_SUPREAD | TLB_FLAG_SUPWRITE | TLB_FLAG_SUPEXECUTE)

/*******************************************************************/

#define BOOTUP_TLB_EA   0xfffff000
#define BOOTUP_TLB_RPN  0xfffff000
#define BOOTUP_SIZE     0x1000

#define BOOTUP_TLB0     (BOOTUP_TLB_EA | TLB_VALID | TLB_SIZE_4KB)
#define BOOTUP_TLB1     ((BOOTUP_TLB_RPN & 0xfffffc00) | (0x1))
#define BOOTUP_TLB2     (TLB_FLAG_UNCACHED | TLB_FLAG_GUARDED | TLB_FLAG_USERREAD | TLB_FLAG_USERWRITE | TLB_FLAG_USEREXECUTE | TLB_FLAG_SUPREAD | TLB_FLAG_SUPWRITE | TLB_FLAG_SUPEXECUTE)

#define BOOTUP_LAST     (BOOTUP_TLB_EA + BOOTUP_SIZE - 4)

/*******************************************************************/

#define STACK_LOCATION     (SRAM_TLB_EA + SRAM_SIZE)
/* offset has to fit into bottom 16 bits and be 16 byte aligned */
#define INTT_OFFSET        (BOOTUP_SIZE - (18 * 4 * 4))

/*******************************************************************/

#define SDR_CUST0_VALUE 0xc0000000

/*******************************************************************/

#define EBC_TLB_EA    0x60000000
#define EBC_TLB_RPN   0xf0000000

#define EBC_TLB0      (EBC_TLB_EA | TLB_VALID | TLB_SIZE_256MB)
#define EBC_TLB1      ((EBC_TLB_RPN & 0xfffffc00) | (0x1))
#define EBC_TLB2      (TLB_FLAG_WRITETHROUGH | TLB_FLAG_UNCACHED | TLB_FLAG_GUARDED | TLB_FLAG_USEREXECUTE | TLB_FLAG_USERREAD | TLB_FLAG_USERWRITE | TLB_FLAG_SUPREAD | TLB_FLAG_SUPWRITE | TLB_FLAG_SUPEXECUTE)

#define EBC_CONFIG         (EBC_CFG_EBTC | EBC_CFG_RTC(7) | EBC_CFG_CSTC)

#define EBC_FLASH_BASE     0x00000000
#define EBC_FLASH_CR       (EBC_TLB_RPN | EBC_FLASH_BASE | EBC_BANK_64M  | EBC_BANK_READ | EBC_BANK_WRITE | EBC_BANK_16B)
#if 0
#define EBC_FLASH_AP       (EBC_AP_TWT(12) | EBC_AP_CSN(0) | EBC_AP_OEN(2) | EBC_AP_WBN(1) | EBC_AP_WBF(3) | EBC_AP_TH(1))
#define EBC_FLASH_AP       (EBC_AP_TWT(6) | EBC_AP_CSN(0) | EBC_AP_OEN(1) | EBC_AP_WBN(1) | EBC_AP_WBF(3) | EBC_AP_TH(1))
#endif
#define EBC_FLASH_AP       (EBC_AP_TWT(13) | EBC_AP_CSN(0) | EBC_AP_OEN(2) | EBC_AP_WBN(1) | EBC_AP_WBF(3) | EBC_AP_TH(2))

#define EBC_CPLD_BASE      0x04000000
#define EBC_CPLD_AP        (EBC_AP_BME | EBC_AP_TWT(0) | EBC_AP_CSN(0) | EBC_AP_OEN(0) | EBC_AP_WBN(0) | EBC_AP_WBF(0) | EBC_AP_TH(0) | EBC_AP_SOR)
#define EBC_CPLD_CR        (EBC_TLB_RPN | EBC_CPLD_BASE  | EBC_BANK_32M  | EBC_BANK_READ | EBC_BANK_WRITE | EBC_BANK_8B)

#define EBC_SM_BASE        0x06000000
#define EBC_SM_AP          (EBC_AP_BME | EBC_AP_TWT(0) | EBC_AP_CSN(0) | EBC_AP_OEN(0) | EBC_AP_WBN(0) | EBC_AP_WBF(0) | EBC_AP_TH(0) | EBC_AP_SOR)
#define EBC_SM_CR          (EBC_TLB_RPN | EBC_SM_BASE  | EBC_BANK_32M  | EBC_BANK_READ | EBC_BANK_WRITE | EBC_BANK_16B)

#define EBC_FPGA_BASE      0x08000000
#define EBC_FPGA_AP        (EBC_AP_TWT(1) | EBC_AP_CSN(0) | EBC_AP_OEN(0) | EBC_AP_WBN(0) | EBC_AP_WBF(0) | EBC_AP_TH(0) | EBC_AP_RE | EBC_AP_SOR)
#define EBC_FPGA_CR        (EBC_TLB_RPN | EBC_FPGA_BASE  | EBC_BANK_128M | EBC_BANK_READ | EBC_BANK_WRITE | EBC_BANK_16B)

#if 0 /* with ready enable */
#define EBC_FPGA_AP        (EBC_AP_TWT(1) | EBC_AP_CSN(0) | EBC_AP_OEN(0) | EBC_AP_WBN(0) | EBC_AP_WBF(0) | EBC_AP_TH(0) | EBC_AP_SOR)
#endif

/*******************************************************************/

#define GPIO0_OSRL_VALUE   0x55055400
#define GPIO0_TSRL_VALUE   0x55055400
#if 0
#define GPIO0_TSRL_VALUE   0x50050000
#endif

/*******************************************************************/

  .file	"crt0.S"
  .text

  .globl _start


  /* Note: registers 3-6 used for emphemeral data */
  /*       register  0 - plain zero */
  /*       register 10 - location of serial port */
  /*       register  9 - copy of PC */
  /*       register  8 - source tlb entry */

  /*       register  7 - to be allocated */

_start:

  /* some basic ops */
  nop
  nop

  /* disable FPU, note that this flag is in the high half */
  lis     r3,CCR0_DAPUIB@h
  mtspr   r_ccr0,r3

  li      r0,0

  iccci   r0,r0
  dccci   r0,r0
  sync

  mtmsr   r0
  mtspr   r_mmucr,r0

  /* make sure that msr stays 0 */
  mtspr   r_mcsrr1,r0
  mtspr   r_csrr1,r0
  mtspr   r_srr1,r0

  bl      _first_position
_first_position: /* WARNING - need to know delta between here and start */
#define START_OFFSET (14 * 4)
  mfspr   r9,r_lr

  /* clear out timers */
  mtspr   r_tcr,r0
  mtspr   r_dec,r0

  /* disable external interrups */
  mtdcr   d_uic0_er,r0
  mtdcr   d_uic0_vcr,r0

  /* know if we returned from an interrupt */
  li      r3,0xded
  mtspr   r_srr0,r3
  mtspr   r_csrr0,r3
  li      r3,0xbef
  mtspr   r_mcsrr0,r3

  mtspr   r_dear,r0

  /* zero out tlb, leaving one which contains our PC */
  li      r3,63
  tlbsx.  r8,0,r9
  beq     _clear_tlb

  /* setup tlb entry 19 if nothing to be found */
  li      r8,0x13
_cover_flash:
  lis     r5,BOOTUP_TLB2@h
  ori     r5,r5,BOOTUP_TLB2@l
  tlbwe   r5,r8,2
  lis     r5,BOOTUP_TLB1@h
  ori     r5,r5,BOOTUP_TLB1@l
  tlbwe   r5,r8,1
  lis     r5,BOOTUP_TLB0@h
  ori     r5,r5,BOOTUP_TLB0@l
  tlbwe   r5,r8,0

_clear_tlb:
  cmpw    r8,r3
  beq     _clear_skip
  tlbwe   r0,r3,0
_clear_skip:
  addic.  r3,r3,-1
  bgt     _clear_tlb

  sync

  /* use tlb entry 16 for serial region */
  li      r4,0x10
  lis     r5,IO_TLB0@h
  ori     r5,r5,IO_TLB0@l
  tlbwe   r5,r4,0
  lis     r5,IO_TLB1@h
  ori     r5,r5,IO_TLB1@l
  tlbwe   r5,r4,1
  lis     r5,IO_TLB2@h
  ori     r5,r5,IO_TLB2@l
  tlbwe   r5,r4,2

  dccci   r0,r0
  sync

  /* serial control */
  li      r4,SDR_SERIAL
  mtdcr   SDR_ADDR,r4

  lis     r4,(SERIAL_CTRL)@h
  ori     r4,r4,(SERIAL_CTRL)@l
  mtdcr   SDR_DATA,r4

  /* load base address of serial port */
  lis     r10,(SERIAL_OUT)@h
  ori     r10,r10,(SERIAL_OUT)@l

  /* make DLL, DLM accessible */
  li      r3,0x80
  stb     r3,SERIAL_LCR(r10)
  li      r3,0x6
  stb     r3,SERIAL_DLL(r10)
  li      r3,0x0
  stb     r3,SERIAL_DLM(r10)
  /* set 8bit mode, make DLL, DLM inaccessible */
  li      r3,0x03
  stb     r3,SERIAL_LCR(r10)

  /* reset both fifos, 0x7 would reset and enable fifo */
  li      r3,0x7
  stb     r3,SERIAL_FCR(r10)
  li      r3,0x0
  stb     r3,SERIAL_MCR(r10)
  stb     r3,SERIAL_SCR(r10)
  /* may need to set this */
  stb     r3,SERIAL_IER(r10)

#ifdef RINIT_INTERACTIVE
  /* print A - the first thing we can */
  li      r3,0x41
  stb     r3,0(r10)
#endif

  /* provisionally make stack end of ram, decrement later */
  lis     r1,(SRAM_TLB_EA + SRAM_SIZE)@h
  ori     r1,r1,(SRAM_TLB_EA + SRAM_SIZE)@l
  addi    r5,r1,-4

  /* check if sram already mapped (needed for stack) */
  tlbsx.  r3,0,r5
  beq     _mapped_ram


#ifdef RINIT_INTERACTIVE
  /* print R for mapping in onboard ram */
  li      r3,0x52
  stb     r3,0(r10)
#endif

  /* use tlb entry 17 for sram */
  li      r4,0x11
  lis     r5,SRAM_TLB2@h
  ori     r5,r5,SRAM_TLB2@l
  tlbwe   r5,r4,2
  lis     r5,SRAM_TLB1@h
  ori     r5,r5,SRAM_TLB1@l
  tlbwe   r5,r4,1
  lis     r5,SRAM_TLB0@h
  ori     r5,r5,SRAM_TLB0@l
  tlbwe   r5,r4,0

  dccci   r0,r0
  sync

_mapped_ram: /* skip mapping of sram */

#ifdef RINIT_INTERACTIVE
  /* print F for flash copy*/
  li      r3,0x46
  stb     r3,0(r10)
#endif

  /* see out of which real page we are running */
  tlbre   r4,r8,1

  /* pull out real address for instruction pointer */
  lis     r5,SRAM_TLB1@h
  ori     r5,r5,SRAM_TLB1@l
  /* WARNING: assumes no parity set in ccr0 */
  cmplw   r4,r5
  beq     _in_ram

  /* destination of copy */
  lis     r5,(SRAM_TLB_EA - 4)@h
  ori     r5,r5,(SRAM_TLB_EA - 4)@l
  /* source of copy */
  addi    r4,r9,(((-1)*START_OFFSET) - 4)
  /* end of copy source */
  addi    r6,r4,BOOTUP_SIZE

_copy_do:
  lwzu    r3,4(r4)
  stwu    r3,4(r5)
  cmplw   r4,r6
  blt     _copy_do

  sync

#ifdef RINIT_INTERACTIVE
  /* print J for jump into ram */
  li      r3,0x4A
  stb     r3,0(r10)
#endif

  lis     r5,SRAM_TLB_EA@h
  ori     r5,r5,SRAM_TLB_EA@l
  addi    r4,r9,(-1)*(START_OFFSET)

  iccci   r0,r0
  dccci   r0,r0

  bl      _second_position
#define FIXUP_OFFSET (6*4)
_second_position: /* WARNING - need to know delta between here and _in_ram */
  mfspr   r9,r_lr

  subf    r3,r4,r9            /* subtract start pos from current */
  add     r9,r5,r3            /* add that difference to start of sram */
  addi    r9,r9,FIXUP_OFFSET  /* fixup between _second_position and _in_ram */
  mtspr   r_lr, r9
  blr    

  /*** break in code, need to get here manually ***/

_in_ram: /* skip copy, code already in ram */

  /* interrupts */

#ifdef RINIT_INTERACTIVE
  /* print i for interrupt init */
  li      r3,0x49
  stb     r3,0(r10)
#endif

  /* interrupt base is start of internal ram */
  lis     r3,SRAM_TLB_EA@h
  ori     r3,r3,SRAM_TLB_EA@l
  mtspr   r_ivpr,r3

  /* interrupt offsets are just below 4k */
  li      r3,INTT_OFFSET
  mtspr   r_ivor0, r3
  addi    r3,r3,16
  mtspr   r_ivor1, r3
  addi    r3,r3,16
  mtspr   r_ivor2, r3
  addi    r3,r3,16
  mtspr   r_ivor3, r3
  addi    r3,r3,16
  mtspr   r_ivor4, r3
  addi    r3,r3,16
  mtspr   r_ivor5, r3
  addi    r3,r3,16
  mtspr   r_ivor6, r3
  addi    r3,r3,16
  mtspr   r_ivor3, r3
  addi    r3,r3,16
  mtspr   r_ivor8, r3
  addi    r3,r3,16
  mtspr   r_ivor9, r3
  addi    r3,r3,16
  mtspr   r_ivor10, r3
  addi    r3,r3,16
  mtspr   r_ivor11, r3
  addi    r3,r3,16
  mtspr   r_ivor12, r3
  addi    r3,r3,16
  mtspr   r_ivor13, r3
  addi    r3,r3,16
  mtspr   r_ivor14, r3
  addi    r3,r3,16
  mtspr   r_ivor15, r3

  /* handle stack setup */
  stwu	  r1,-16(r1)            	/* transform end of sram to real stack pointer */
  stw	  r0,0(r1)			/* set end of back chain */

#ifdef RINIT_INTERACTIVE
  /* print e for ebc setup*/
  li      r3,0x45
  stb     r3,0(r10)
#endif

  /* use pins for chip select, not gpio */
  lis     r4,IO_TLB_EA@h
  ori     r4,r4,IO_TLB_EA@l
  
  lis     r3,GPIO0_OSRL_VALUE@h
  ori     r3,r3,GPIO0_OSRL_VALUE@l
  stw     r3,GPIO0_OSRL(r4)
  /* also do tristate */
  lis     r3,GPIO0_TSRL_VALUE@h
  ori     r3,r3,GPIO0_TSRL_VALUE@l
  stw     r3,GPIO0_TSRL(r4)

  /* disable nand flash controller */
  li      r4,SDR0_CUST0
  mtdcr   d_sdraddr,r4
  lis     r3,SDR_CUST0_VALUE@h
  ori     r3,r3,SDR_CUST0_VALUE@l
  mtdcr   d_sdrdata,r3

  /* setup EBC config */
  li      r4,EBC_CFG
  mtdcr   d_ebcaddr,r4
  lis     r3,EBC_CONFIG@h
  ori     r3,r3,EBC_CONFIG@l
  mtdcr   d_ebcdata,r3

  /* setup SM  bank */
  li      r4,EBC_B4AP
  mtdcr   d_ebcaddr,r4
  lis     r3,EBC_SM_AP@h
  ori     r3,r3,EBC_SM_AP@l
  mtdcr   d_ebcdata,r3
  li      r4,EBC_B4CR
  mtdcr   d_ebcaddr,r4
  lis     r3,EBC_SM_CR@h
  ori     r3,r3,EBC_SM_CR@l
  mtdcr   d_ebcdata,r3

  /* setup CPLD bank */
  li      r4,EBC_B2AP
  mtdcr   d_ebcaddr,r4
  lis     r3,EBC_CPLD_AP@h
  ori     r3,r3,EBC_CPLD_AP@l
  mtdcr   d_ebcdata,r3

  li      r4,EBC_B2CR
  mtdcr   d_ebcaddr,r4
  lis     r3,EBC_CPLD_CR@h
  ori     r3,r3,EBC_CPLD_CR@l
  mtdcr   d_ebcdata,r3

  /* set up FPGA bank */
  li      r4,EBC_B1AP
  mtdcr   d_ebcaddr,r4
  lis     r3,EBC_FPGA_AP@h
  ori     r3,r3,EBC_FPGA_AP@l
  mtdcr   d_ebcdata,r3
  li      r4,EBC_B1CR
  mtdcr   d_ebcaddr,r4
  lis     r3,EBC_FPGA_CR@h
  ori     r3,r3,EBC_FPGA_CR@l
  mtdcr   d_ebcdata,r3

  /* set up flash bank */
  li      r4,EBC_B0AP
  mtdcr   d_ebcaddr,r4
  lis     r3,EBC_FLASH_AP@h
  ori     r3,r3,EBC_FLASH_AP@l
  mtdcr   d_ebcdata,r3
  li      r4,EBC_B0CR
  mtdcr   d_ebcaddr,r4
  lis     r3,EBC_FLASH_CR@h
  ori     r3,r3,EBC_FLASH_CR@l
  mtdcr   d_ebcdata,r3

  /* use tlb entry 18 for EBC */
  li      r4,0x12
  lis     r5,EBC_TLB2@h
  ori     r5,r5,EBC_TLB2@l
  tlbwe   r5,r4,2
  lis     r5,EBC_TLB1@h
  ori     r5,r5,EBC_TLB1@l
  tlbwe   r5,r4,1
  lis     r5,EBC_TLB0@h
  ori     r5,r5,EBC_TLB0@l
  tlbwe   r5,r4,0

  /* TODO: clear out caching stuff (but all our regions are uncached ?) */

#ifdef RINIT_INTERACTIVE
  /* print m */
  li      r3,0x4d
  stb     r3,0(r10)
#endif

  bl      main

_prog_end:
  b       _prog_end
  b       _prog_end

