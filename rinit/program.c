/* Rinit: The roach init. A simple bootloader */

/* Marc Welz <marc@ska.ac.za> */
/* Shanly Rajan <shanly.rajan@ska.ac.za> */
/* Dave George <dgeorgester@gmail.com> */
/* Brandon Hamilton <brandon.hamilton@gmail.com> */


#include "serial.h"
#include "lib.h"
#include "flash.h"
#include "selectmap.h"

#define COPY_SIZE            4096
/* Note that the following have to match the TLB entries in crt0.S */
#define FLASH_POS     (FLASH_END - COPY_SIZE)
#define RAM_POS        0x71000000
#define CPLD_POS       0x64000000
#define SM_POS         0x66000000

#define DELAY(n) for(z=0; z < n * 60000; z++); 

int flash_erase(unsigned int start_addr)
{
	unsigned int i;

	for(i = start_addr; i < CPLD_POS; i = i + 0x20000)
	{
		if (erase_sector_flash(i)){
			print_string("bad addr\r\n");
			return 1;
		}
		else{
			poll_flash();
			/*	print_string("Done\r\n");*/
		}
	}
	return 0;
}

int transfer_image(unsigned int write_loc)
{

	struct xmodem xm;
	int result,z;

	init_xmodem(&xm);

	/*	print_string("rx:TAKE UR TIME:press CTRL+A,s,choose xmodem and the file to send ");*/

	result = getblock_xmodem(&xm); 
	while (!result) {
		write_flash(write_loc, (unsigned int)(xm.x_buffer), XMODEM_BLOCK);
		write_loc += XMODEM_BLOCK;
		result = getblock_xmodem(&xm);
	}

	DELAY(5);

	/*	print_string("\r\n\r\n\r\n\r\n\r\n");*/

	if(result < 0){
		print_string("bad\r\n");
	} else {
		print_string("ok\r\n");
	}
	return 0;
}

int reset_board(unsigned short address, unsigned char data)
{
	//*((volatile unsigned char *)(CFG_CPLD_BASE + address)) = data;
	*((volatile unsigned char *)(CPLD_POS + address)) = data;

	return 0;
}


int main()
{
	int z;

	/*print_string("A few messages away to get hands dirty on u-boot\r\n");*/

#ifdef RINIT_INTERACTIVE
	print_string("Erasing Flash Sectors\r\n");
#endif

	/*erase the flash sectors for programming u-boot and FPGA bit image*/
	if(flash_erase(0x63fa0000))
		return 1;

	/*Initiate xmodem transfer of u-boot and FPGA bit image to avoid loading from usb*/
	/*print_string("Preparing to Transfer u-boot\r\n");*/
	if(transfer_image(0x63fa0000))
		return 1;

	DELAY(5);

	/*print_string("\n\n\n\n\n");*/

	/*Issue soft reset by writing to the resp CPLD reg for u-boot to come up*/
#ifdef RINIT_INTERACTIVE
	print_string("Reset Roach board\r\n");
#endif
	if(reset_board(0x0000 , 0xFF))
		return 1;


	return 0;
}

