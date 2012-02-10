/*
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

#include <common.h>
#include <command.h>
#include <fat.h>
#include <usb.h>

#include <configs/roach.h>
#include <roach/roach_cpld.h>
#include <roach/selectmap.h>
#include <roach/roach_bsp.h>

#if (CONFIG_COMMANDS & CFG_CMD_ROACH)
#if (defined(CFG_SMAP_BASE) && defined(CFG_FPGA_BASE) && defined(CFG_CPLD_BASE))

#define BITFILE_FILENAME "roach_bsp.bin"
#define SCRATCHPAD_A     0x200000

#define USB_DEV_STRING      "0:1"
#define USB_INTERFACE_STRING "usb"

#define SM_INITB_WAIT 100000
#define SM_DONE_WAIT  100000

unsigned char bitswap(unsigned char in)
{
  return (in & 0x1  << 7)  | (in & 0x2  << 5) |
         (in & 0x4  << 3)  | (in & 0x8  << 1) |
         (in & 0x10 >> 1)  | (in & 0x20 >> 3) |
         (in & 0x40 >> 5)  | (in & 0x80 >> 7);
}

unsigned short shortbytesex(unsigned short in)
{
  return ((in & 0xff) << 8) | ((in & 0xff00) >> 8);
}

int selectmap_program(unsigned int addr, unsigned int length)
{
  int i;
  int byte_swap = 0;
  volatile unsigned int *src,*dst;
  volatile unsigned short *src_s,*dst_s;

  if (*((unsigned char *)(addr + 34)) == 0x00 && *((unsigned char *)(addr + 35)) == 0xBB){
    debug("The .bin file is byte reversed\n");
    byte_swap = 1;
  } else if ((*((unsigned char *)(addr + 34))) != 0xBB || (*((unsigned char *)(addr + 35))) != 0x00){
    printf("error: the file doesn't appear to be a .bin file -- %x %x\n", *((unsigned char *)(addr + 34)), *((unsigned char *)(addr + 35)));
    return -1;
  }
  

  /* disable init_n output -- let pullup do the work */
  *((volatile unsigned char *) (CFG_CPLD_BASE + CPLD_SM_CTRL)) = (unsigned char)(0x00);

  /* RD_WR_N set to 0 [WR], INIT_N to 1, PROG_B to 1 */
  *((volatile unsigned char *) (CFG_CPLD_BASE + CPLD_SM_OREGS)) = (unsigned char)(0x03);

  
  for (i=0; i < 32; i++){ /* Hold for at least 350ns */
    /* RD_WR_N set to 0 [WR], INIT_N to 0, PROG_B to 0 */
    *((volatile unsigned char *) (CFG_CPLD_BASE + CPLD_SM_OREGS )) = (unsigned char)(0x00);
  }

  /* RD_WR_N set to WR, INIT_N to 1, PROG_B to 1 */
  *((volatile unsigned char *) (CFG_CPLD_BASE + CPLD_SM_OREGS)) = (unsigned char)(0x03);

  for (i=0; i < SM_INITB_WAIT + 1; i++){
    if ((*((volatile unsigned char *) (CFG_CPLD_BASE + CPLD_SM_STATUS))) & CPLD_SM_INIT_N){
      break;
    }
    if (i == SM_INITB_WAIT){
#ifdef DEBUG
      printf("sm:init_n stuck low\n");
#endif
      return -1;
    }
  }
  
  if (byte_swap){
    src_s = (unsigned short *)(addr);
    dst_s = (unsigned short *)(CFG_SMAP_BASE);

    for(i = 0; i < length; i+=2){
      *dst_s = shortbytesex(*src_s);
      src_s++;
    }
  } else {
    src = (unsigned int *)(addr);
    dst = (unsigned int *)(CFG_SMAP_BASE);

    for(i = 0; i < length; i+=4){
      *dst = *src;
      src++;
    }
  }

  for (i=0; i < SM_INITB_WAIT + 1; i++){
    if ((*((volatile unsigned char *) (CFG_CPLD_BASE + CPLD_SM_STATUS))) & CPLD_SM_DONE){ //done high
      break;
    }
    if (i == SM_DONE_WAIT){
#ifdef DEBUG
      printf("sm: done low\n");
#endif
      return 1;
    }
  }

  return 0;
}

#define DEFAULT_IMAGE_SIZE  3889856 

int do_roachsmap(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
  ulong addr, length;

  length = DEFAULT_IMAGE_SIZE;

  if (argc < 1) {
    printf ("Usage:\n%s\n", cmdtp->usage);
    return 1;
  }

  addr = simple_strtoul(argv[1], NULL, 16);
  if(argc > 2){
     length = simple_strtoul(argv[2], NULL, 10);
  }

  printf("source %x (%d bytes)\n", addr, length);

  /* at this point we need to use the code from Dave's rinit 
   * to set up the FPGA and then copy data from addr to the
   * selectmap chipselect io region (this still needs to be set up)
   */

  return selectmap_program(addr, length);
}

U_BOOT_CMD(
	roachsmap,	3,	1,	do_roachsmap,
	"roachsmap - write to selectmap\n",
	"address [length]\n"
        "        - source address with optional length\n"
);

/************************************************************************/

int load_file_from_usb(const char * filename, unsigned int target)
{
  long size;
  unsigned long offset;
  unsigned long count;
  block_dev_desc_t *dev_desc=NULL;
  int dev=0;
  int part=1;
  char *ep;

  dev = (int)simple_strtoul (USB_DEV_STRING, &ep, 16);
  dev_desc = get_dev(USB_INTERFACE_STRING, dev);

  if (dev_desc==NULL) {
    puts ("\n** Invalid boot device **\n");
    return 1;
  }

  if (*ep) { // not entirely sure what this is about
    if (*ep != ':') {
      puts ("\n** Invalid boot device, use `dev[:part]' **\n");
      return 1;
    }
    part = (int)simple_strtoul(++ep, NULL, 16);
  }

  if (fat_register_device(dev_desc,part)!=0) {
    printf ("\n** Unable to use %s %d:%d for fatload **\n",USB_INTERFACE_STRING,dev,part);
    return 1;
  }

  offset = target;
  count = 0; //read whole file, not partial

  size = file_fat_read (filename, (unsigned char *) offset, count);
  if(size==-1) {
    printf("\n** Unable to read \"%s\" from %s %d:%d **\n",filename,USB_INTERFACE_STRING,dev,part);
    return 1;
  }
  
  return 0;
}

static int foo = -1;

void wait(int n)
{
  volatile unsigned int i;
  for (i = 0; i < n; i++){}
  return;
}

int usb_setup(void)
{
  int ret;
  usb_stop();
  ret = usb_init();
  if (ret >= 0)
  	foo = usb_stor_scan(1);
  return ret;
}

/*************************************************** Tests ************************************************/

static int test_selectmap(void)
{
  if (usb_setup()){
    printf("usb start error\n");
    return -1;
  } else if (load_file_from_usb(BITFILE_FILENAME, SCRATCHPAD_A)){
    printf("usb load file error\n");
    return -1;
  } else if (selectmap_program(SCRATCHPAD_A, V5LX110_BITFILE_SIZE)) {
    printf("selectmap failed\n");
    return -1;
  } else if (usb_stop()){
    printf("usb stop error\n");
    return -1;
  } else {
    return 0;
  }
}

/************************************************************************/

static int test_cpld(void)
{
  if (*((unsigned short *) (CFG_CPLD_BASE + CPLD_REV_ID)) != CPLD_ID) {
    printf("unexpected ID %x\n", *((unsigned short *) (CFG_CPLD_BASE + CPLD_REV_ID)));
    return -1;
  }
  printf("ID = %x, REV_MAJOR = %x, REV_MINOR = %x, REV_RCS = %x\n",
                  *((unsigned short *) (CFG_CPLD_BASE + CPLD_REV_ID)),
                  *((unsigned char  *) (CFG_CPLD_BASE + CPLD_REV_MAJOR)),
                  *((unsigned char  *) (CFG_CPLD_BASE + CPLD_REV_MINOR)),
                  *((unsigned short *) (CFG_CPLD_BASE + CPLD_REV_RCS)));
  return 0;
}

int do_roachcpld(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
  return test_cpld();
}

U_BOOT_CMD(
	roachcpld,	1,	1,	do_roachcpld,
	"roachcpld - check cpld identifier\n",
	"- test if CPLD is available\n"
);

/************************************************************************/

static int test_roachbsp(void)
{
  unsigned short test;
  if (*((volatile unsigned short *) (CFG_FPGA_BASE + RBSP_SYS_ID)) != ROACHBSP_ID) {
    printf("unexpected ID %x\n", *((volatile unsigned short *) (CFG_FPGA_BASE + RBSP_SYS_ID)));
    return -1;
  }

  printf("ID = %x, REV_MAJOR = %x, REV_MINOR = %x, REV_RCS = %x\n",
                  *((volatile unsigned short *) (CFG_FPGA_BASE + RBSP_SYS_ID)),
                  *((volatile unsigned short *) (CFG_FPGA_BASE + RBSP_SYS_MAJOR)),
                  *((volatile unsigned short *) (CFG_FPGA_BASE + RBSP_SYS_MINOR)),
                  *((volatile unsigned short *) (CFG_FPGA_BASE + RBSP_SYS_RCS)));
  /* Write a test Pattern and read it back */
  *(volatile unsigned short *) (CFG_FPGA_BASE + RBSP_SYS_SCRATCH) = 0x1234;
  if ((test = *((volatile unsigned short *) (CFG_FPGA_BASE + RBSP_SYS_SCRATCH))) != 0x1234) {
    printf("invalid readback - got %x, expected %x\n", test, 0x1234);
    return -1;
  }

  return 0;
}

int do_roachfpga(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
  return test_roachbsp();
}

U_BOOT_CMD(
	roachfpga,	1,	1,	do_roachfpga,
	"roachfpga - write test pattern to fpga\n",
	"- checks inteface to FPGA\n"
);


/************************************************************************/

#define DRAM_DATA_LENGTH 18
#define DRAM_TEST_LENGTH (1024*1024)
#define DRAM_ADDR_OFFSET 0

static void dram_get_value_indirect(unsigned int addr, unsigned short *data)
{
  int i;
  unsigned int offset = CFG_FPGA_BASE + DRAM_OFFSET;
  *((volatile unsigned int   *) (offset + RBSP_DRAM_ADDR)) = addr;
  *((volatile unsigned short *) (offset + RBSP_DRAM_RDEN)) = 0x1;
  wait(5);
  for (i=0; i < DRAM_DATA_LENGTH; i++){
    *(data + i) = *((volatile unsigned short *)(offset + RBSP_DRAM_RDDATA(i)));
  }
  return;
}

static void dram_set_value_indirect(unsigned int addr, const unsigned short *data)
{
  int i;
  unsigned int offset = CFG_FPGA_BASE + DRAM_OFFSET;
  *((volatile unsigned int *) (offset + RBSP_DRAM_ADDR)) = addr;
  *((volatile unsigned int *) (offset + RBSP_DRAM_MASK0)) = 0xffffffff; /* enable all data bits (active high) */
  *((volatile unsigned int *) (offset + RBSP_DRAM_MASK1)) = 0xffffffff; /* enable all data bits (active high) */

  for (i=0; i < DRAM_DATA_LENGTH; i++){
    *((volatile unsigned short *)(offset + RBSP_DRAM_WRDATA(i))) = *(data + i);
  }
  *((volatile unsigned short *) (offset + RBSP_DRAM_WREN)) = 0x1;
  return;
}

static void dram_reset(void)
{
  unsigned int offset;

  offset = CFG_FPGA_BASE + DRAMC_OFFSET;

  (*(volatile unsigned short *) (offset + RBSP_DRAM_RESET)) = 0x1;
  return;
}

static unsigned short dram_get_status(void)
{
  unsigned int offset;

  offset = CFG_FPGA_BASE + DRAMC_OFFSET;

  return (*((volatile unsigned short *) (offset + RBSP_DRAM_PHYSTATUS)));
}

static unsigned short dram_get_freq(void)
{
  unsigned int offset;

  offset = CFG_FPGA_BASE + DRAMC_OFFSET;

  return (*((volatile unsigned short *) (offset + RBSP_DRAM_FREQ)));
}

#define DRAM_TEST_VALUE 0x1234

static int test_roachbsp_dram(void)
{
  int i, ret, n = 0;
  unsigned short data_in  [DRAM_DATA_LENGTH];
  unsigned short data_out [DRAM_DATA_LENGTH];

  unsigned short fail_bitmask = 0;

  if ((ret = dram_get_freq()) == 0 || ret > 400) {
    printf("error: invalid DRAM Frequency - %d\n", ret);
    return -1;
  } else {
    debug("DRAM Frequency: %d\n", ret);
  }

  if (!(dram_get_status() & RBSP_DRAM_PHYRDY)) {
    debug("DRAM PHY not ready, resetting\n");
    /* Write a test Pattern and read it back */
    dram_reset();
    wait(100000);
    if (!(dram_get_status() & RBSP_DRAM_PHYRDY)) {
      printf("error: DRAM PHY not ready\n");
      return -1;
    } else {
      debug("DRAM PHY training complete\n");
    }
  } else {
    debug("DRAM PHY training complete\n");
  }

  if (dram_get_status() & RBSP_DRAM_CALFAIL) {
    printf("error: DRAM PHY calibration failed\n");
    return -1;
  }

  n=0;
  while (n < DRAM_TEST_LENGTH) {
    for (i = 0; i < DRAM_DATA_LENGTH; i++){
      ret = DRAM_TEST_VALUE + i + n*(DRAM_DATA_LENGTH);
      data_out[i] = ret;
    }
    dram_set_value_indirect((unsigned int)(n) << 2, data_out);
    dram_get_value_indirect((unsigned int)(n) << 2, data_in);

    ret = 0;
    for (i = 0; i < DRAM_DATA_LENGTH; i++){
      if (data_out[i] != data_in[i]){
        if (i == 8 || i == 17) {
          ret = 1;
        } else {
          ret = -1;
          fail_bitmask |= data_in[i] ^ data_out[i];
          break; //break out of compare loop
        }
      }
    }
    if (ret < 0)
      break; //break out of while loop
    n++;
  }

  if (ret < 0){
    printf("DRAM memory check failed at address: 0x%x\n", n);
    printf("  write data: ");
    for (i = 0; i < DRAM_DATA_LENGTH; i++){
      printf("  %d:%04x", i, data_out[i]);
    }
    printf("\n");
    printf("  read data:  ");
    for (i = 0; i < DRAM_DATA_LENGTH; i++){
      printf("  %d:%04x", i, data_in[i]);
    }
    printf("\n");
    printf("  xor'ed:     ");
    for (i = 0; i < DRAM_DATA_LENGTH; i++){
      printf("  %d:%04x", i, data_in[i] ^ data_out[i]);
    }
    printf("\n");
    printf("  fail mask:  ");
    printf("  %04x", fail_bitmask);
    printf("\n");
    return -1;
  }

  if (ret) {
    printf("dram transfer test completed successfully [64 bit device]\n");
  } else {
    printf("dram transfer test completed successfully [72 bit device]\n");
  }
  
  return 0;
}

int do_roachddr(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
  return test_roachbsp_dram();
}

U_BOOT_CMD(
	roachddr,	1,	1,	do_roachddr,
	"roachddr  - ddr test\n",
	"- checks DIMM attached to FPGA\n"
);

/************************************************************************/

#define QDR_DATA_LENGTH 4
#define QDR_TEST_LENGTH 32768
#define QDR_ADDR_OFFSET 0

static void qdr_get_value_indirect(int which, unsigned int addr, unsigned short *data)
{
  int i;
  unsigned int offset = CFG_FPGA_BASE + (which ? QDR1_OFFSET : QDR0_OFFSET);
  *((volatile unsigned int   *) (offset + RBSP_QDR_ADDR)) = addr;
  *((volatile unsigned short *) (offset + RBSP_QDR_RDEN)) = 0x1;
  wait(5);
  for (i=0; i < QDR_DATA_LENGTH; i++){
    *(data + i) = *((volatile unsigned short *)(offset + RBSP_QDR_RDDATA(i)));
  }
  return;
}

static void qdr_set_value_indirect(int which, unsigned int addr, const unsigned short *data)
{
  int i;
  unsigned int offset = CFG_FPGA_BASE + (which ? QDR1_OFFSET : QDR0_OFFSET);
  *((volatile unsigned int   *) (offset + RBSP_QDR_ADDR)) = addr;
  *((volatile unsigned short *) (offset + RBSP_QDR_MASK)) = 0xff; /* enable all data bits (active high) */

  for (i=0; i < QDR_DATA_LENGTH; i++){
    *((volatile unsigned short *)(offset + RBSP_QDR_WRDATA(i))) = *(data + i);
  }
  *((volatile unsigned short *) (offset + RBSP_QDR_WREN)) = 0x1;
  return;
}

static void qdr_reset(int which)
{
  unsigned int offset;

  offset = CFG_FPGA_BASE + (which ? QDRC1_OFFSET : QDRC0_OFFSET);

  (*(volatile unsigned short *) (offset + RBSP_QDR_RESET)) = 0x1;
  return;
}

static unsigned short qdr_get_status(int which)
{
  unsigned int offset;

  offset = CFG_FPGA_BASE + (which ? QDRC1_OFFSET : QDRC0_OFFSET);

  return (*((volatile unsigned short *) (offset + RBSP_QDR_PHYSTATUS)));
}

static unsigned short qdr_get_freq(int which)
{
  unsigned int offset;

  offset = CFG_FPGA_BASE + (which ? QDRC1_OFFSET : QDRC0_OFFSET);

  return (*((volatile unsigned short *) (offset + RBSP_QDR_FREQ)));
}

#define QDR_TEST_VALUE 0x1234

static int test_roachbsp_qdr(int which)
{
  int i, ret, n = 0;
  unsigned short data_in  [QDR_DATA_LENGTH];
  unsigned short data_out [QDR_DATA_LENGTH];

  unsigned short fail_bitmask = 0;

  if ((ret = qdr_get_freq(which)) == 0 || ret > 400) {
    printf("error: invalid QDR Frequency - %d\n", ret);
    return -1;
  } else {
    debug("QDR Frequency: %d\n", ret);
  }

  if (!(qdr_get_status(which) & RBSP_QDR_PHYRDY)) {
    debug("QDR PHY not ready, resetting\n");
    /* Write a test Pattern and read it back */
    qdr_reset(which);
    wait(100000);
    if (!(qdr_get_status(which) & RBSP_QDR_PHYRDY)) {
      printf("error: QDR PHY not ready\n");
      return -1;
    } else {
      debug("QDR PHY training complete\n");
    }
  } else {
    debug("QDR PHY training complete\n");
  }

  if (qdr_get_status(which) & RBSP_QDR_CALFAIL) {
    printf("error: QDR PHY calibration failed\n");
    return -1;
  }

  n=0;
  while (n < QDR_TEST_LENGTH) {
    for (i = 0; i < QDR_DATA_LENGTH; i++){
      ret = QDR_TEST_VALUE + i + n*(QDR_DATA_LENGTH);
      data_out[i] = ret;
    }
    qdr_set_value_indirect(which, (unsigned int)(n), data_out);
    qdr_get_value_indirect(which, (unsigned int)(n), data_in);

    ret = 0;
    for (i = 0; i < QDR_DATA_LENGTH; i++){
      if (data_out[i] != data_in[i]){
        ret = -1;
        //break; //break out of compare loop
        fail_bitmask |= data_in[i] ^ data_out[i];
      }
    }
    n++;
  }
  if (ret){
    printf("QDR memory check failed at address: 0x%x\n", n);
    printf("  write data: ");
    for (i = 0; i < QDR_DATA_LENGTH; i++){
      printf("  %d:%04x", i, data_out[i]);
    }
    printf("\n");
    printf("  read data:  ");
    for (i = 0; i < QDR_DATA_LENGTH; i++){
      printf("  %d:%04x", i, data_in[i]);
    }
    printf("\n");
    printf("  xor'ed:     ");
    for (i = 0; i < QDR_DATA_LENGTH; i++){
      printf("  %d:%04x", i, data_in[i] ^ data_out[i]);
    }
    printf("\n");
    printf("  fail mask:  ");
    printf("  %04x", fail_bitmask);
    printf("\n");
    return -1;
  }
  
  printf("qdr%d transfer test completed successfully\n", which);
  return 0;
}

int do_roachqdr(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
  int which;

  if(argc > 1){
    which = (int)simple_strtoul(argv[1], NULL, 10);
  } else {
    which = 0;
  }

  return test_roachbsp_qdr(which);
}

U_BOOT_CMD(
	roachqdr,	2,	1,	do_roachqdr,
	"roachqdr  - qdr transfer test\n",
	"- optional parameter selects the device number\n"
);

/************************************************************************/

#define CONFIG_NORMAL      0x0
#define CONFIG_INTERLEAVED 0x1
#define CONFIG_RAMP        0x2
#define CONFIG_FIXED       0x3

static void iadc_stop(int which)
{
  const unsigned int offset = CFG_FPGA_BASE + (which ? IADC1_OFFSET: IADC0_OFFSET); 
  //stop the ADC
  *((volatile unsigned short*)(offset + RBSP_IADC_FIFO_CTRL)) = 0x0;
}

static void iadc_start(int which)
{
  const unsigned int offset = CFG_FPGA_BASE + (which ? IADC1_OFFSET: IADC0_OFFSET); 
  //start the ADC
  *((volatile unsigned short*)(offset + RBSP_IADC_FIFO_CTRL)) = 0xffff;
}

#define TWI_WAIT_TIMEOUT 10000

int iadc_twi_wait(int which)
{
  int i;
  const unsigned int offset = CFG_FPGA_BASE + (which ? IADC1_OFFSET: IADC0_OFFSET); 
  for (i=0; i < TWI_WAIT_TIMEOUT; i++){
    if ((*((volatile unsigned short*)(offset + RBSP_IADC_TWI_TX))) == 0x0){
      return 0;
    }
  }
  return -1;

}

void iadc_twi_set(int which, unsigned short addr, unsigned short data)
{
  const unsigned int offset = CFG_FPGA_BASE + (which ? IADC1_OFFSET: IADC0_OFFSET); 
  iadc_twi_wait(which);
  *((volatile unsigned short*)(offset + RBSP_IADC_TWI_ADDR)) = addr;
  *((volatile unsigned short*)(offset + RBSP_IADC_TWI_DATA)) = data;
  *((volatile unsigned short*)(offset + RBSP_IADC_TWI_TX))   = 0x1;
}

static void iadc_setup(int which, int config, int data)
{
  const unsigned int offset = CFG_FPGA_BASE + (which ? IADC1_OFFSET: IADC0_OFFSET); 
  *((volatile unsigned short*)(offset + RBSP_IADC_MODE)) = 0x0;
  wait(100);
  *((volatile unsigned short*)(offset + RBSP_IADC_MODE)) = 0xffff;
  wait(100);

  switch (config){
    case CONFIG_NORMAL:
      iadc_twi_set(which, 0x0, 0x7cbc);
      break;
    case CONFIG_INTERLEAVED:
      iadc_twi_set(which, 0x0, 0x7c2c);
      break;
    case CONFIG_RAMP:
      iadc_twi_set(which, 0x0, 0x3c2c);     /* set config reg to interleaved + /2 clock */
      iadc_twi_set(which, 0x6, 0x3);        /* set built-in-test to ramp */
      iadc_twi_set(which, 0x7, data & 0x7); /* set delay adjust to data ps */
      break;
    case CONFIG_FIXED:
      iadc_twi_set(which, 0x0, 0x7c2c); /* set config reg to interleaved + /4 clock */
      iadc_twi_set(which, 0x6, 0x1 | (data << 2));    /* set built-in-test to ramp */
      break;
  }
  wait(1000);
  *((volatile unsigned short*)(offset + RBSP_IADC_RESET)) = 0x1;
  wait(100);
  iadc_stop(which);
  iadc_start(which);
}

static void iadc_getdata(int which, unsigned char* data, int len){
  int i,j;
  const unsigned int offset = CFG_FPGA_BASE + (which ? IADC1_OFFSET: IADC0_OFFSET); 
  for (i=0; i < len/8; i++){
    *(((volatile unsigned short*)(offset + RBSP_IADC_FIFO_ADV))) = 0xffff; /* tick the fifo over */
    for (j=0; j < 8; j++){
      data[i*8 + j] = *(((volatile unsigned char*)(offset + RBSP_IADC_FIFODATA(1)+j))); /* skip the sync/overflow data */
    } /* NOTE: this will go beyond the 'len' bounds */
  }
}

#define DATA_LENGTH (1024*8)

int iadc_counter_test(int which, int delay)
{
  unsigned char data[DATA_LENGTH];
  int i,j,k;
  for (k=0; k < 16; k++){
    iadc_setup(which, CONFIG_RAMP, delay & 0x3);
    wait(1000);
    iadc_stop(which);
    iadc_getdata(which, data, DATA_LENGTH);

    for (i=0; i < (DATA_LENGTH/8) - 1; i++){
      if ((unsigned char)(data[0 + 8*i] - 1) != data[2 + 8*i] ||
          (unsigned char)(data[2 + 8*i] - 1) != data[0 + 8*(i+1)]){
        debug("counter test failed chan i0: i = %d - %d, %d, %d\n", i, data[0 + 8*i], data[2 + 8*i], data[0 + 8*(i+1)]);
        for (j=0; j < 32; j++){
          debug("%02x ", data[i*8 + j - 16]);
        }
        debug("\n");
        return -1;
      }

      if ((unsigned char)(data[1 + 8*i] + 1) != data[3 + 8*i] ||
          (unsigned char)(data[3 + 8*i] + 1) != data[1 + 8*(i+1)]){
        debug("counter test failed chan i1: i = %d - %d, %d, %d\n", i, data[1 + 8*i], data[3 + 8*i], data[1 + 8*(i+1)]);
        for (j=0; j < 32; j++){
          debug("%02x ", data[i*8 + j - 16]);
        }
        debug("\n");
        return -1;
      }

      if ((unsigned char)(data[4 + 8*i] - 1) != data[6 + 8*i] ||
          (unsigned char)(data[6 + 8*i] - 1) != data[4 + 8*(i+1)]){
        debug("counter test failed chan q0: i = %d - %d, %d, %d\n", i, data[4 + 8*i], data[6 + 8*i], data[4 + 8*(i+1)]);
        for (j=0; j < 32; j++){
          debug("%02x ", data[i*8 + j - 16]);
        }
        debug("\n");
        return -1;
      }

      if ((unsigned char)(data[5 + 8*i] + 1) != data[7 + 8*i] ||
          (unsigned char)(data[7 + 8*i] + 1) != data[5 + 8*(i+1)]){
        debug("counter test failed chan q1: i = %d - %d, %d, %d\n", i, data[5 + 8*i], data[7 + 8*i], data[5 + 8*(i+1)]);
        for (j=0; j < 32; j++){
          debug("%02x ", data[i*8 + j - 16]);
        }
        debug("\n");
        return -1;
      }
    }
  }
  return 0;
}

int iadc_continuity_test(int which)
{
  unsigned char data[DATA_LENGTH];
  int i, j, k;
  unsigned char n;
  /* Fixed pattern/continuity test */
  for (i=0; i < 16; i++){ /* 16 test bytes: 1 << (0..7) , ~(1 << (0..7) */
    n = (i < 8 ? (1 << i) : ~(1 << (i - 8))); /* test byte */
    iadc_setup(which, CONFIG_FIXED, n);
    iadc_stop(which);
    iadc_getdata(which, data, 64);

    iadc_getdata(which, data, 8);
    for (j=0; j < 8; j++){
      if (((j%2 == 0) && (data[j] != ((~n) & 0xff))) || ((j%2 == 1) && (data[j] != n))){ /* iadc returns alternates ~s */
        debug("continuity check failed:  (test byte = %x, offset = %d", n, j);
        for (k=0; k < 8; k++){
          printf("  %d:%02x", k, data[k]);
        }
        printf("\n");
        return -1;
      }
    }
  }
  return 0;
}

static int test_roachbsp_adc(int which)
{
  volatile unsigned int test;
  int i, ret;

  const unsigned int offset = CFG_FPGA_BASE + (which ? IADC1_OFFSET: IADC0_OFFSET); 
  /* check whether the 32-bit clock tick register is ticking
   * note: this is can have false positive results
   */
  if (*((volatile unsigned int*)(offset + RBSP_IADC_CLKTEST1)) == *((volatile unsigned int*)(offset + RBSP_IADC_CLKTEST1))){
    /* if check fails, check for slow clock */
    test = *((volatile unsigned int*)(offset + RBSP_IADC_CLKTEST1));
    wait(10000); 
    if (test == *((volatile unsigned int*)(offset + RBSP_IADC_CLKTEST1))){
      printf("no ADC clock present\n");
      return -1;
    }
  }

  /* Fixed pattern/continuity test */
  if (iadc_continuity_test(which)){
    printf("Continuity check failed\n");
    return -1;
  } else {
    printf("Continuity check passed\n");
  }

  /* Fixed pattern/continuity test */
  ret = -1;
  for (i=0; i < 8; i++){
    if (!iadc_counter_test(which, i)){
      ret = 0;
      debug("Counter check passed - delay = %d\n", i);
      break;
    }
  }
  if (ret){
    printf("Counter check failed\n");
    return -1;
  } else {
    printf("Counter check passed\n");
  }
 /*
  * dump values from both channels
  */
#if 0
  iadc_setup(which, CONFIG_NORMAL, 0);
  iadc_getdata(which, data, DATA_LENGTH);
  printf("adc%d data snapshot (300 samples):\n", which);
  for (i=0; i < 15; i++){
    for (j=0; j < 20; j++){
      printf("%02x ", data[i*20 + j]);
    }
    printf("\n");
  }
#endif
  return 0;
}

int do_roachadc(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
  int which;

  if(argc > 1){
    which = (int)simple_strtoul(argv[1], NULL, 10);
  } else {
    which = 0;
  }

  return test_roachbsp_adc(which);
}

U_BOOT_CMD(
	roachadc,	2,	1,	do_roachadc,
	"roachadc  - captures 300 samples from adc\n",
	"- optional parameter selects the device number\n"
);

/************************************************************************/

static int tge_test_link(int which)
{
  int i;
  unsigned int offset;
  int ret;
  switch (which){
    case 0:
      offset = CFG_FPGA_BASE + TGE0_OFFSET; 
      break;
    case 1:
      offset = CFG_FPGA_BASE + TGE1_OFFSET; 
      break;
    case 2:
      offset = CFG_FPGA_BASE + TGE2_OFFSET; 
      break;
    default:
      offset = CFG_FPGA_BASE + TGE3_OFFSET; 
      break;
  }
  for (i=0; i < 100000; i++){
    if (!((ret = *((volatile unsigned short*)(offset + RBSP_XAUI_LINKSTATUS))) & 0x40)){ 
      return -1;
    }
  }
  return 0;
}

static int tge_search_link(int which)
{
  int ret;
  unsigned int offset;
  int i;

  switch (which){
    case 0:
      offset = CFG_FPGA_BASE + TGE0_OFFSET; 
      break;
    case 1:
      offset = CFG_FPGA_BASE + TGE1_OFFSET; 
      break;
    case 2:
      offset = CFG_FPGA_BASE + TGE2_OFFSET; 
      break;
    case 3 :
      offset = CFG_FPGA_BASE + TGE3_OFFSET; 
      break;
    default:
      printf("unsupported device number %d\n", which);
      return 1;
  }
  for (i=0; i < 100000; i++){
    if (!((ret = *((volatile unsigned short*)(offset + RBSP_XAUI_LINKSTATUS))) & 0x40)){ 
      return -1;
    }
  }
  return 0;
}


static int test_roachbsp_tge(int which)
{
  unsigned int offset;

  switch (which){
    case 0:
      offset = CFG_FPGA_BASE + TGE0_OFFSET; 
      break;
    case 1:
      offset = CFG_FPGA_BASE + TGE1_OFFSET; 
      break;
    case 2:
      offset = CFG_FPGA_BASE + TGE2_OFFSET; 
      break;
    default:
      offset = CFG_FPGA_BASE + TGE3_OFFSET; 
      break;
  }
  #if 0
  if (!((ret = *((volatile unsigned short*)(offset + RBSP_TGE_LINKSTATUS))) & 0x40)){ 
    printf("link down - %x\n", ret);
    return -1;
  }
  #endif
  if (!tge_test_link(which)){
    printf("link up\n");
  } else if (!tge_search_link(which)){
    printf("link up\n");
  } else {
    printf("link down\n");
    return -1;
  }
  return 0;
}

int do_roach10ge(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
  int which;

  if(argc > 1){
    which = (int)simple_strtoul(argv[1], NULL, 10);
  } else {
    which = 0;
  }

  return test_roachbsp_tge(which);
}

U_BOOT_CMD(
	roach10ge,	2,	1,	do_roach10ge,
	"roach10ge - checks link on 10ge interface\n",
	"- optional parameter selects the device number\n"
);

/************************************************************************/

int do_roachtest(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
  printf("-------------------------------\n");
  printf("       ROACH Test Suite        \n");
  printf("-------------------------------\n\n");

  printf("ROACH CPLD communications test:\n");
  if (test_cpld()) {
    printf("\n                             failed\n\n");
  } else {
    printf("\n                             passed\n\n");
  }

#if 0
  printf("ROACH CPLD MMC Test: \n");
  printf("not implemented\n\n");
#endif

  printf("ROACH SelectMap test:\n");
  if (test_selectmap()) {
    printf("\n                             failed\n\n");
  } else {
    printf("\n                             passed\n\n");
  }
    
  wait(1000000);

  printf("ROACH BSP communications test:\n");
  if (test_roachbsp()) {
    printf("\n                             failed\n\n");
  } else {
    printf("\n                             passed\n\n");
  }

  printf("ROACH basic DRAM test:\n");
  if (test_roachbsp_dram()) {
    printf("\n                             failed\n\n");
  } else {
    printf("\n                             passed\n\n");
  }

  printf("ROACH basic QDR 0 test:\n");
  if (test_roachbsp_qdr(0)) {
    printf("\n                             failed\n\n");
  } else {
    printf("\n                             passed\n\n");
  }

  printf("ROACH basic QDR 1 test:\n");
  if (test_roachbsp_qdr(1)) {
    printf("\n                             failed\n\n");
  } else {
    printf("\n                             passed\n\n");
  }

  printf("ROACH ADC 0 test:\n");
  if (test_roachbsp_adc(0)) {
    printf("\n                             failed\n\n");
  } else {
    printf("\n                             passed\n\n");
  }

  printf("ROACH ADC 1 test:\n");
  if (test_roachbsp_adc(1)) {
    printf("\n                             failed\n\n");
  } else {
    printf("\n                             passed\n\n");
  }

  printf("ROACH Ten Gigabit Ethernet 0 test:\n");
  if (test_roachbsp_tge(0)) {
    printf("\n                             failed\n\n");
  } else {
    printf("\n                             passed\n\n");
  }

  printf("ROACH Ten Gigabit Ethernet 1 test:\n");
  if (test_roachbsp_tge(1)) {
    printf("\n                             failed\n\n");
  } else {
    printf("\n                             passed\n\n");
  }

  printf("ROACH Ten Gigabit Ethernet 2 test:\n");
  if (test_roachbsp_tge(2)) {
    printf("\n                             failed\n\n");
  } else {
    printf("\n                             passed\n\n");
  }

  printf("ROACH Ten Gigabit Ethernet 3 test:\n");
  if (test_roachbsp_tge(3)) {
    printf("\n                             failed\n\n");
  } else {
    printf("\n                             passed\n\n");
  }
  return 0;
}

/***************************************************/

U_BOOT_CMD(
	roachtest,	1,	1,	do_roachtest,
	"roachtest - run roach tests\n",
	"performs roach test functions\n"
);

#endif /* CFG_SMAP_BASE && CFG_FPGA_BASE && CFG_CPLD_BASE */
#endif /* CFG_CMD_ROACHTEST */
