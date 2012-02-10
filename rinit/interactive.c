/* Rinit: The roach init. A simple bootloader */

/* Marc Welz <marc@ska.ac.za> */
/* Shanly Rajan <shanly.rajan@ska.ac.za> */
/* Dave George <dgeorgester@gmail.com> */
/* Brandon Hamilton <brandon.hamilton@gmail.com> */

/**** configurable settings */
//#define ADLER /* enable checksum */
#undef ADLER
#undef  FPGA  /* enable fpga programming */
/**** end of configurable settings */

#include "serial.h"
#include "lib.h"
#include "flash.h"
#include "selectmap.h"

#define COPY_SIZE            4096
/*SHANLY SPECIFIC*/
#define ALIGN(a) (a & 0xFFFFFFFE)
#define DELAY(n) for(z=0;z<n*60000;z++);
/* Note that the following have to match the TLB entries in crt0.S */
#define FLASH_POS     (FLASH_END - COPY_SIZE)
#define RAM_POS        0x71000000
#define CPLD_POS       0x64000000
#define SM_POS         0x66000000

/* Adler checksum code based on wikipedia example */
#ifdef ADLER
#define MOD_ADLER 65521


#if 0
#define FLASH_LOCATION(a)            (*(volatile unsigned short *)(FLASH_BASE + ((a) * 2)))
#define FLASH_VALUE(v)               ((v) & 0xff)
#define FLASH_EXTRACT(a)             FLASH_VALUE(FLASH_LOCATION(a))
#endif

unsigned int adler(unsigned char *data, unsigned int len)
{
  unsigned int a = 1, b = 0;

  while (len > 0)
  {
    unsigned int tlen = len > 5550 ? 5550 : len;
    len -= tlen;
    do
    {
      a += *data++;
      b += a;
    } while (--tlen);

    a %= MOD_ADLER;
    b %= MOD_ADLER;
  }

  return (b << 16) | a;
}
#endif

int main()
{
  unsigned int c, a, b;
  struct xmodem xm;
  int result;
  void (*j)();

  print_string("G\r\n");
  a = b = 0;

  for(;;){
    c = test_char();
    switch(c){
      case 'a' :
        print_string("A: ");
        print_hex_unsigned(a);
        print_string("\r\nA: ");
        a = get_hex_unsigned(a);
        break;
      case 'b' :
        b = a;
        break;
#ifdef ADLER
      case 'c' :

        if(a >= b){
          break;
        }

        print_string("L: ");
        print_hex_unsigned(b - a);
        print_string("\r\nC: ");
        print_hex_unsigned(adler((void *)a, (b - a)));
        print_newline();
        
        break;
#endif
      case 'W':
        a += 4;
        /* Falling through */
      case 'w':
        *((volatile unsigned int *) a) = b;
        flush();
        break;
      case 'x' :
        if(a < FLASH_BASE){
          print_string("A not flash\r\n");
          continue;
        }

        init_xmodem(&xm);
        b = a;
        print_string("rx: ");

        result = getblock_xmodem(&xm); 
        while (!result) {
          write_flash(b, (unsigned int)(xm.x_buffer), XMODEM_BLOCK);
          b += XMODEM_BLOCK;
          result = getblock_xmodem(&xm);
        }

        if(result < 0){
          print_string("bad\r\n");
        } else {
          print_string("ok\r\n");
        }
        break;
      case 'R' :
        a += 4;
        /* fall */
      case 'r' :
        print_hex_unsigned(a);
        print_string(": ");
        print_hex_unsigned(*((unsigned int *)a));
        print_newline();
        break;
      case 'i' :
        print_string("A: ");
        print_hex_unsigned(a);
        print_newline();
        print_string("B: ");
        print_hex_unsigned(b);
        print_newline();
        break;
	  case 'z' :
		print_string("Resetting ROACH:");
		*((volatile unsigned char *)(CPLD_POS + 0x0000)) = 0xFF;
		break;

#if 0
      case 'h' : 
      case '?' : 
#ifdef LONG_HELP
        print_string("h - help\r\n");
        print_string("E - erase\r\n");
        print_string("i - display pointers\r\n");
        print_string("w - display pointers\r\n");
#else
        print_string("pick jhEibawW\r\n");
#endif
        break;
#endif
      case 'E' :
        print_string("erase ... ");
        erase_chip_flash();
        print_string("done\r\n");
        break;
      case 'e' :
	print_string("erase sector ... ");
        if (erase_sector_flash(a))
          print_string("bad addr\r\n");
        else
  	  print_string("done\r\n");
	break;
      case 'j' :
        print_string("jumping ... ");
        j = (void *)a;
        (*j)();
        break;
      case 's' :
        a = FLASH_POS;
        /* fall */
      case 'S' :
        if(a < FLASH_BASE){
          print_string("A not flash\r\n");
          continue;
        }
        print_string("saving ... ");
        write_flash(a, RAM_POS, COPY_SIZE);
        print_string("done\r\n");
        break;
	  case 't':
		/*Put Flash in CFI mode and check for response*/
        print_string("CFI Try\r\n");
		//*((volatile unsigned short *)(FLASH_BASE + 0x000)) = (unsigned short)(0x00FF);/*CFI Exit  Mode*/
		//*((volatile unsigned short *)(FLASH_BASE + ALIGN(0x55))) = (unsigned short)(0x0098);/*CFI Entry Mode*/
		*((volatile unsigned short *)(FLASH_BASE + 0xAA)) = (unsigned short)(0x0098);/*CFI Entry Mode*/
        print_hex_unsigned(*((unsigned short *)(FLASH_BASE + ALIGN(0x20))));
        print_string(": ");
        print_hex_unsigned(*((unsigned short *)(FLASH_BASE + ALIGN(0x22))));
        print_string(": ");
        print_hex_unsigned(*((unsigned short *)(FLASH_BASE + ALIGN(0x24))));
		*((volatile unsigned short *)(FLASH_BASE + 0x000)) = (unsigned short)(0x00F0);/*CFI Exit  Mode*/
        print_string("\r\nDone\r\n");
		break;
#ifdef FPGA
      case 'm' :
        print_string("sb ");
        a = CPLD_POS + CPLD_SM_STATUS;
        print_hex_unsigned(*((unsigned char *)a));
        print_string("\r\n");

        /* RD_WR_N set to 0 [WR], INIT_N to 1, PROG_B to 1 */
        *((volatile unsigned char *) CPLD_POS + CPLD_SM_OREGS) = (unsigned char)(0x03);
        /* enable init_n output */
        *((volatile unsigned char *) CPLD_POS + CPLD_SM_CTRL ) = (unsigned char)(0x01);

        /* RD_WR_N set to WR, INIT_N to 1, PROG_B to 0 */
        for (i=0; i < 32; i++){
        /* Hold for at least 350ns */
          *((volatile unsigned char *) CPLD_POS + CPLD_SM_OREGS) = (unsigned char)(0x02);
        }

        /* RD_WR_N set to WR, INIT_N to 0, PROG_B to 0 */
        *((volatile unsigned char *) CPLD_POS + CPLD_SM_OREGS) = (unsigned char)(0x00);
        /* RD_WR_N set to WR, INIT_N to 0, PROG_B to 1 */
        *((volatile unsigned char *) CPLD_POS + CPLD_SM_OREGS) = (unsigned char)(0x01);
        /* RD_WR_N set to WR, INIT_N to 1, PROG_B to 1 */
        *((volatile unsigned char *) CPLD_POS + CPLD_SM_OREGS) = (unsigned char)(0x03);
        /* enable init_n input */
        *((volatile unsigned char *) CPLD_POS + CPLD_SM_CTRL ) = (unsigned char)(0x00);

        init_xmodem(&xm);
        result = getblock_xmodem(&xm); 
        b = 0;
        while (!result) {
          for (i=0; i < 128; i+=2){
            /* Dump all the xmodem stuff - extras at the end wont do any damage
             * marginal save on the checking...*/
            #ifdef ROACH_V0
            *((volatile unsigned short *) SM_POS) =
                 SM_BITHACK( (((unsigned short)(*(xm.x_buffer + i + 0))) << 0) |
                             (((unsigned short)(*(xm.x_buffer + i + 1))) << 8) );
            #else
            *((volatile unsigned short *) SM_POS) =
                          (((unsigned short)(*(xm.x_buffer + i + 0))) << 0) |
                          (((unsigned short)(*(xm.x_buffer + i + 1))) << 8);
            #endif
          }
          result = getblock_xmodem(&xm);
          b++;
        }
        if(result < 0){
          print_string("e\r\n");
        } else {
          print_string("sa ");
          b = CPLD_POS + CPLD_SM_STATUS;
          print_hex_unsigned(*((unsigned char *)b));
          print_string(" \r\n");
        }
        break;
#endif
      case 'k':
        print_string("resetting ... ");
        reset_flash();
        print_string("done\r\n");
        break;
      case 'p':
        print_string("polling ...\r\n");
        poll_flash();
        print_string(" done\r\n");
        break;
      case SERIAL_TIMEOUT :
      default :
        continue;
    }

  }

  return 0;
}
