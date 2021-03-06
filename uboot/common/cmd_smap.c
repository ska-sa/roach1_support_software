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

//#include "cmd_roachtest.c"

#include <common.h>
#include <command.h>

#if (CONFIG_COMMANDS & CFG_CMD_ROACH)
#if (defined(CFG_SMAP_BASE) && defined(CFG_FPGA_BASE) && defined(CFG_CPLD_BASE))

#define DEFAULT_IMAGE_SIZE  3889856 

int do_smap(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
    
	ulong addr, length;

        length = DEFAULT_IMAGE_SIZE;

	if (argc < 1) {
		printf ("Usage:\n%s\n", cmdtp->usage);
		return 1;
	}

        addr = simple_strtoul(argv[1], NULL, 16);
/*
        if(argc > 1){
            length = simple_strtoul(argv[2], NULL, 10);
        }
*/
        printf("source %x (%d bytes)\n", addr, length);

        /* at this point we need to use the code from Dave's rinit 
         * to set up the FPGA and then copy data from addr to the
         * selectmap chipselect io region (this still needs to be set up)
         */

        selectmap_program(addr,length);
        //selectmap_program(0x200000,3889856);

        return 0;
}

/***************************************************/

U_BOOT_CMD(
	smap,	2,	1,	do_smap,
	"smap    - write to selectmap\n",
	"address [length]\n"
        "        - source address with optional length\n"
);

#endif /* FPGA, CPLD and SMAP base */
#endif /* CFG_CMD_ROACH */
