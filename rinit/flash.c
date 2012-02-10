/* Simple flash programmer for rinit */
/* Marc Welz <marc@ska.ac.za> */
/* Shanly Rajan <shanly.rajan@ska.ac.za> */
/* Brandon Hamilton <brandon.hamilton@gmail.com> */

#include "lib.h"
#include "flash.h"

#define FLASH_CMD_CFI            0x98
#define FLASH_CMD_RESET          0xf0
#define FLASH_CMD_U1             0xaa
#define FLASH_CMD_U2             0x55
#define FLASH_CMD_SETUP          0x80
#define FLASH_CMD_CHIPERASE      0x10
#define FLASH_CMD_CHIPERASE      0x10
#define FLASH_CMD_SECTOR_ERASE   0x30
#define FLASH_CMD_SINGLEWORD     0xa0
#define FLASH_CMD_UNLOCK_BYPASS  0x20
#define FLASH_CMD_EXIT_BYPASS    0x90

#define FLASH_REQUEST_UNLOCK1    0xAAA
#define FLASH_REQUEST_UNLOCK2    0x554
#define FLASH_REQUEST_CFI        0x55

#define FLASH_CFI_Q              0x10
#define FLASH_CFI_R              0x11
#define FLASH_CFI_Y              0x12
#define FLASH_CFI_VENDOR_LO      0x13
#define FLASH_CFI_VENDOR_HI      0x14
#define FLASH_CFI_SIZE           0x27

#define SECTOR_START(address) (address & 0xFFFE0000)

#define MAX_TRIES                    1000000

void poll_flash()
{
  volatile unsigned short *c0;
  unsigned short s, t;

  c0 = (unsigned short *)(FLASH_BASE);

  t = *c0;

  do{
    s = t;
    t = *c0;

#if RINIT_INTERACTIVE
    print_hex_unsigned(s);
    print_string("\r");
#endif
    
  } while(t != s);
}

int erase_chip_flash()
{
  volatile unsigned short *c1, *c2;
#if 0
  flash_location(0) =  FLASH_CMD_RESET;
  flush();
#endif

  c1 = (unsigned short *)(FLASH_BASE + FLASH_REQUEST_UNLOCK1);
  c2 = (unsigned short *)(FLASH_BASE + FLASH_REQUEST_UNLOCK2);
#if 0
  c0 = (unsigned short *)(FLASH_BASE);
#endif

  *c1 = FLASH_CMD_U1;
  barrier();
  *c2 = FLASH_CMD_U2;
  flush();
  *c1 = FLASH_CMD_SETUP;
  flush();
  *c1 = FLASH_CMD_U1;
  barrier();
  *c2 = FLASH_CMD_U2;
  flush();
  /* conflict: datasheet says two things here */
  *c1 = FLASH_CMD_CHIPERASE;
  flush();

  return 0;
}

/*
 * The sector erase function erases a sector in the memory array.
 */
int erase_sector_flash(unsigned int address)
{
  volatile unsigned short *c1, *c2, *c3;

  c1 = (unsigned short *)(FLASH_BASE + FLASH_REQUEST_UNLOCK1);
  c2 = (unsigned short *)(FLASH_BASE + FLASH_REQUEST_UNLOCK2);
  c3 = (unsigned short *)(SECTOR_START(address));

  if((address < FLASH_BASE) || (address > FLASH_END)){
    return -1;
  }

  *c1 = FLASH_CMD_U1;
  barrier();
  *c2 = FLASH_CMD_U2;
  flush();
  *c1 = FLASH_CMD_SETUP;
  flush();
  *c1 = FLASH_CMD_U1;
  barrier();
  *c2 = FLASH_CMD_U2;
  flush();
  *c3 =  FLASH_CMD_SECTOR_ERASE;
  flush();

  return 0;
}

void reset_flash()
{
  volatile unsigned short *c0, *c1, *c2;

  c1 = (unsigned short *)(FLASH_BASE + FLASH_REQUEST_UNLOCK1);
  c2 = (unsigned short *)(FLASH_BASE + FLASH_REQUEST_UNLOCK2);
  c0 = (unsigned short *)(FLASH_BASE);

  *c1 = FLASH_CMD_U1;
  barrier();
  *c2 = FLASH_CMD_U2;
  flush();
  *c1 = FLASH_CMD_RESET;
  flush();
  *c0 = FLASH_CMD_RESET;
  flush();
}

int write_flash(unsigned int dst, unsigned int src, unsigned int n)
{ 
  unsigned int i, j;
  volatile unsigned short *ram, *flash, *c1, *c2;

  c1 = (unsigned short *)(FLASH_BASE + FLASH_REQUEST_UNLOCK1);
  c2 = (unsigned short *)(FLASH_BASE + FLASH_REQUEST_UNLOCK2);

  if((dst < FLASH_BASE) || (dst > FLASH_END)){
    return -1;
  }

  *c1 = FLASH_CMD_RESET;
  flush();

  ram = (unsigned short *)(src);
  flash = (unsigned short *)(dst);

#if 0
  *c1 = FLASH_CMD_U1;
  barrier();
  *c2 = FLASH_CMD_U2;
  flush();
  *c1 = FLASH_CMD_UNLOCK_BYPASS;
  barrier();
#endif

  for(i = 0; i < n; i += 2){

    *c1 = FLASH_CMD_U1;
    barrier();
    *c2 = FLASH_CMD_U2;
    flush();
    *c1 = FLASH_CMD_SINGLEWORD;
    barrier();
    *flash = *ram;
    flush();

    for(j = 0; (j < MAX_TRIES) && ((*flash) != (*ram)); j++);

    flash++;
    ram++;
  }

#if 0
  *c1 = FLASH_CMD_EXIT_BYPASS;
  barrier();
  *c1 = 0;
  flush();
#endif

  return 0;
}

