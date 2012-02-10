/* Spansion MirrorBit NOR Flash driver (S29GL-P) */
/* Brandon Hamilton <brandon.hamilton@gmail.com> */

#include "lib.h"
#include "flash.h"

#define FLASH_16BIT 
/* Flash commands */
#define FLASH_CMD_RESET              0x00F0
#define FLASH_CMD_AUTOSELECT         0x0090
#define FLASH_CMD_PROGRAM_SETUP      0x00A0
#define FLASH_CMD_WRITE_BUFFER_LOAD  0x0025
#define FLASH_CMD_WRITE_CONFIRM      0x0029
#define FLASH_CMD_ERASE_SETUP        0x0080
#define FLASH_CMD_SECTOR_ERASE       0x0030
#define FLASH_CMD_CHIP_ERASE         0x0010
#define FLASH_CMD_SUSPEND            0x00B0
#define FLASH_CMD_RESUME             0x0030
#define FLASH_CMD_UNLOCK_BYPASS      0x0020
#define FLASH_CMD_CFI_QUERY          0x0098

#define FLASH_OP_WRITE_BUFFER  0x01
#define FLASH_OP_PROGRAMMING   0x02
#define FLASH_OP_ERASE         0x04

#define SECTOR_START(address) (address & 0xFFFF0000)
#define SECTOR_ADDRESS(sector) ((((sector) << 17) & 0xFFFF0000))
#define ALIGN(a) (a & 0xFFFFFFFE)
#define DELAY(n) for(z=0;z<n*60000;z++);

#if 0
#define FLASH_LOCATION(a)            (*(volatile unsigned short *)(FLASH_BASE + ((a) * 2)))
#define FLASH_VALUE(v)               ((v) & 0xff)
#define FLASH_EXTRACT(a)             FLASH_VALUE(FLASH_LOCATION(a))
#endif

char unlock_bypass = 0;
void write_flash_unlock_sequence()
{
  if (!unlock_bypass)
  {
#ifdef FLASH_BYTE_PIN
    /* Write unlock cycle 1 */
    *( (volatile unsigned short *) (FLASH_BASE + 0x554)) = 0xAAAA;
    /* Write unlock cycle 2 */
    *( (volatile unsigned short *) (FLASH_BASE + 0x2AA)) = 0x5555;
#else
    /* Write unlock cycle 1 */
    *( (volatile unsigned short *) (FLASH_BASE + 0xAAA)) = 0xAA;
    /* Write unlock cycle 2 */
    *( (volatile unsigned short *) (FLASH_BASE + 0x554)) = 0x55;
#endif
  }
}

void write_flash_command(unsigned char command)
{
#ifdef FLASH_BYTE_PIN
  /* Write command */
  *( (volatile unsigned short *) (FLASH_BASE + 0x554)) = command;
#else
  /* Write command */
  *( (volatile unsigned short *) (FLASH_BASE + 0xAAA)) = command; 
#endif
}

int poll_status(char operation_mode, unsigned long address, unsigned short data)
{
#if 0
  unsigned short valid_code;
  unsigned long timeout;
  int z;
  int i = 0;

  switch(operation_mode)
  {
    case FLASH_OP_WRITE_BUFFER:
    case FLASH_OP_PROGRAMMING:
      /* Write */
      valid_code = data & (1 << 7);
      timeout = FLASH_WRITE_TIMEOUT;
      DELAY(1);
      break;
    case FLASH_OP_ERASE:
      /* Erase */
      valid_code = (1 << 7);
      timeout = FLASH_ERASE_TIMEOUT;
      DELAY(100);
      break;
    default:
      print_string("bad op\r\n");
      return -6;
  }

  while ((*((volatile unsigned short *)(FLASH_BASE + address)) & (1 << 7)) != valid_code)
  {
    if(i++ > timeout){
      print_string(" tmout\r\n");
      return (-1);
    }
    /* print_string("."); */
  }

  /* print_string("\r\n"); */
#endif

  return 0;
}

#ifdef UNTESTED_POLLING
/* Poll write/erase operation status
 * Return values:
 *  0  Successful
 * -1  Timeout
 * -2  Write Buffer operation failed
 * -3  Program operation failed
 * -4  Device Error
 * -5  Suspend mode
 * -6  Invalid operation mode
 */
int poll_status(char operation_mode, unsigned long address, unsigned short data)
{
  unsigned short read1;
  unsigned short read2;
  unsigned short read3;
  
  unsigned short valid_code;
  
  switch(operation_mode)
  {
    case FLASH_OP_WRITE_BUFFER:
    case FLASH_OP_PROGRAMMING:
      /* Erase */
      valid_code = data & (1 << 7);
      break;
    case FLASH_OP_ERASE:
      /* Write */
      valid_code = (1 << 7);
      break;
    default:
      return -6;
  }

  unsigned long aladd = ALIGN(address);
  /* Polling loop */
  while(1)
  {
    read1 = *( (volatile unsigned short *) (FLASH_BASE + aladd));

    /* Valid */
    if ((read1 & (1 << 7)) == valid_code)
    {
      read2 = *( (volatile unsigned short *) (FLASH_BASE + aladd) );
      read3 = *( (volatile unsigned short *) (FLASH_BASE + aladd) );

      if (operation_mode & FLASH_OP_PROGRAMMING)
      {
        if (read3 == valid_code)
        {
          /* Program operation complete */
          return 0;
        }
        else
        {
          /* Program operation failed */
          return -3;
        }
      }
      else
      {
        if ((read2 & (1 << 6)) != (read3 & (1 << 6)))
        {
          /* Device error */
          return -4;
        }
        else
        {
          if ((read2 & (1 << 2)) != (read3 & (1 << 2)))
          {
            /* Suspend Mode */
            return -5;
          }
          else
          {
            /* Erase operation complete */
            return 0;
          }
        }
      }
    }
    /* Not valid */
    else
    {
      if (read1 & (1 << 5))
      {
        read2 = *( (volatile unsigned short *) (FLASH_BASE + aladd) );
        read3 = *( (volatile unsigned short *) (FLASH_BASE + aladd) );
        if ((read2 & (1 << 6)) != (read3 & (1 << 6)))
        {
          /* Timeout */
          return -1;
        }
        else
        {
          /* Device is busy */
          continue;
        }
      }
      else
      {
        /* Write buffer programming ? */
        if (operation_mode & FLASH_OP_WRITE_BUFFER)
        {
          if (read1 & (1 << 1))
          {
            read2 = *( (volatile unsigned short *) (FLASH_BASE + aladd) );
            read3 = *( (volatile unsigned short *) (FLASH_BASE + aladd) );

            if ((read3 & (1 << 1)) && (read1 & (1 << 7)) == valid_code)
            {
              /* Write buffer operation failed */
              return -2;
            }
            else
            {
              /* Device is busy */
              continue;	
            }
          }
          else
          {
            /* Device is busy */
            continue;
          }
        }
        else
        {
          /* Device is busy */
          continue;
        }
      }
    }
  }

  return 0;
}
#endif

#define USE_SECTOR_ERASE 1
/*
 * The sector erase function erases one or more sectors in the memory array.
 */
#ifdef USE_SECTOR_ERASE
int erase_sector_flash(unsigned int start_sector, unsigned int end_sector)
{
  //reset_flash();
  int i, z;

  print_string("Erasing ... ");

  write_flash_unlock_sequence();
  write_flash_command((unsigned char) FLASH_CMD_ERASE_SETUP);
  write_flash_unlock_sequence();
  
  /* Sector erase */
  for (i = start_sector; i <= end_sector; i++)
  {
    //print_string("Erasing sector at address: ");
    //print_hex_unsigned(FLASH_BASE + SECTOR_ADDRESS(i));
    //print_string("\r\n");
    *((volatile unsigned short *) (FLASH_BASE + SECTOR_ADDRESS(i))) = FLASH_CMD_SECTOR_ERASE;
  }
  DELAY(500);
  if (poll_status(FLASH_OP_ERASE, SECTOR_ADDRESS(i), 0) != 0)
  {
    write_flash_command((unsigned char) FLASH_CMD_RESET);
    return -1;
  }
  print_string("ok\r\n");
  return 0;
}
#endif

/*
 * The sector erase function erases one or more sectors in the memory array.
 */
//#define USE_ERASE_CHIP
#ifdef USE_ERASE_CHIP
int erase_chip_flash()
{
  int z;
  reset_flash();
  write_flash_unlock_sequence();
  write_flash_command((unsigned char) FLASH_CMD_ERASE_SETUP);
  write_flash_unlock_sequence();
  write_flash_command((unsigned char) FLASH_CMD_CHIP_ERASE);
  DELAY(200);
  /*if (poll_status(FLASH_OP_ERASE, SECTOR_ADDRESS(0x00), 0) != 0)
  {
    write_flash_command((unsigned char) FLASH_CMD_RESET);
    return -1;
  }*/
  print_string("OK\r\n");
  return 0;
}
#endif

/*
 * The Erase Suspend command allows the system to interrupt a sector erase operation and then read data
 * from, or program data to, any sector not selected for erasure.
 */
#ifdef ALLOW_SUSPEND
void suspend_flash_erase()
{
  *( (volatile unsigned short *) FLASH_BASE) = FLASH_CMD_SUSPEND;	
}

void resume_flash_erase(unsigned int sector)
{
  *( (volatile unsigned short *) SECTOR_ADDRESS(sector)) = FLASH_CMD_RESUME;	
}

/*
 * The Program Suspend command allows the system to interrupt a write operation and then read data
 * from, or program data to, any sector not selected for writing.
 */

void suspend_flash_write()
{
  *( (unsigned short *) FLASH_BASE) = FLASH_CMD_SUSPEND;	
}

void resume_flash_write()
{
  *( (unsigned short *) FLASH_BASE) = FLASH_CMD_RESUME;	
}
#endif

/* Single word programming mode is one method of programming the Flash. In this mode, four Flash command
 * write cycles are used to program an individual Flash address. The data for this programming operation 
 * could be 8 or 16-bits wide.
 * 
 * Not recommended if Write Buffer programming is available
 *
 */
#define  USE_SINGLE_WORD_PROGRAM
#ifdef USE_SINGLE_WORD_PROGRAM
int single_word_program_flash(unsigned long address, unsigned short data)
{
  write_flash_unlock_sequence();
  write_flash_command((unsigned char) FLASH_CMD_PROGRAM_SETUP);
  *( (volatile unsigned short *) (FLASH_BASE + ALIGN(address))) = data;
  
  /*if (poll_status(FLASH_OP_PROGRAMMING, ALIGN(address), data) != 0)
  {
    write_flash_command((unsigned char) FLASH_CMD_RESET);
    return -1;
  }*/
  int z;
  DELAY(5);
  if (*( (volatile unsigned short *) (FLASH_BASE + ALIGN(address))) != data)
    return -1;
  
  return 0;
}
#endif
/*            
 * Write buffer programming limited to 16 words. 
 * All addresses to be written to the flash in one operation must be within the same flash   
 * page. A flash page begins at addresses evenly divisible by 0x20.
 */

int write_flash(unsigned long address, unsigned short *src, unsigned short n)
{ 
  int i,z;
  unsigned long sector_address = SECTOR_START(ALIGN(address));

  //reset_flash();

  write_flash_unlock_sequence();
  
  /* sadly xmodem requires silence to operate */
  /* print_string("Writing Sector Address: "); */
  /* print_hex_unsigned(sector_address); */

  /* Write*/ 
  *( (volatile unsigned short *) (FLASH_BASE + sector_address)) = FLASH_CMD_WRITE_BUFFER_LOAD;
  *( (volatile unsigned short *) (FLASH_BASE + sector_address)) = n-1;

  unsigned short *write_address = (unsigned short *) (FLASH_BASE + ALIGN(address));
  for (i = 0; i < n; i++)
  {
    *write_address++ = *src++;
    /* print_string("+"); */
  }
  
  *((volatile unsigned short *) (FLASH_BASE + sector_address)) = FLASH_CMD_WRITE_CONFIRM; 
  /* print_string("done\r\n"); */
  DELAY(20);
  if (poll_status(FLASH_OP_WRITE_BUFFER, *(write_address-1), *(src-1)) != 0)
  {
    //write_flash_command((unsigned char) FLASH_CMD_RESET);
    return -1;
  }
  
  return 0;
}


/* Read data from flash */
#ifdef USE_READ_FLASH
int read_flash(unsigned long address, unsigned short *dest, unsigned short n)
{
  int i;
  unsigned short *src = (unsigned short *)(FLASH_BASE + (ALIGN(address)));
  for (i = 0; i< n; i++)
  {
    *dest++ = *src++;
  }
  return 0;
}
#endif

/* Soft reset */
void reset_flash()
{
  write_flash_command((unsigned char) FLASH_CMD_RESET);
}

#ifdef UNLOCK_BYPASS
/*
 * Unlock bypass mode.
 * Enter this mode before doing a sequence of similar program
 * operations. After this call, programming only takes two write 
 * cycles.
 * Once you enter Unlock Bypass Mode, do a series of like
 * operations (programming or sector erase) and then exit
 * Unlock Bypass Mode before beginning a different type of
 * operations.
 */
void unlock_bypass_flash()
{
  write_flash_unlock_sequence();
  write_flash_command((unsigned char) FLASH_CMD_UNLOCK_BYPASS);
  unlock_bypass = 1;
}

void unlock_bypass_reset_flash()
{
  /* Reset cycle 1 */
  *( (volatile unsigned short *) FLASH_BASE ) = 0x0090;
  /* Reset cycle 2 */
  *( (volatile unsigned short *) FLASH_BASE ) = 0x0000;
  unlock_bypass = 0;
}
#endif

/*
void test_flash_erase()
{
	int i,j;
        unsigned short verify[8];
	unsigned short test_pattern[] = { 0x1111, 0x2222, 0x3333, 0x4444, 0x5555, 0x6666, 0x7777, 0x8888 };
	print_string("Writing buffer2flash...");
	if (0 != write_flash(0x60, test_pattern, 8))
	  print_string("failed\r\n");
	else
	  print_string("successful\r\n");
	reset_flash();
	print_string("Verifying write...");
	j = 0;
	read_flash(0x60, verify, 8);
	for (i = 0; i < 8; i++)
	{
		if (verify[i] != test_pattern[i])
			j = 1;
		
		print_hex_unsigned(verify[i]);
		print_string(":");
	}
	if (j)
		 print_string("...failed\r\n");
	else
		 print_string("...successful\r\n");
	reset_flash();


	if (!erase_sector_flash(0x00, 0x1f))
          print_string("Erasing sectors successfull");
        else
          print_string("Erasing sectors failed");

	print_string("Checking that data was erased in sector ");
	j = 0;

	read_flash(0x60, verify, 8);
	for (i = 0; i < 8; i++)
	{
		if (verify[i] != test_pattern[i])
			j = 1;
		
		print_hex_unsigned(verify[i]);
	if (j)
		 print_string("...failed\r\n");
	else
		 print_string("...successful\r\n");
	reset_flash();
       
}*/


#ifdef USE_TEST_FLASH
void test_flash_erase()
{

/*
  unsigned short verify[16];
  long address_to_write; 
print_string("STARTB4 TEST PATTERN\r\n");
 unsigned short test_pattern[] = { 0x1111, 0x2222, 0x3333, 0x4444, 0x5555, 0x6666, 0x7777, 0x8888 };
//unsigned short test_pattern[] = { 0x11112222, 0x33334444, 0x55556666, 0x77778888, 0x99998888, 0x77776666, 0x55554444, 0x33332222};
 unsigned short verify[8];
 
 print_string("START TO TEST\r\n");
 for (c = 0x50; c < 0x54; c++)
 {
         reset_flash();
         address_to_write = SECTOR_ADDRESS(c) + 20;
         print_string("Writing buffer to flash in sector ");
         print_hex_unsigned(c);
         print_string("...");
         
         if (0 != write_flash(address_to_write, test_pattern, 8))
                 print_string("failed\r\n");
         else
                 print_string("successful\r\n");
                 
         print_string("Verifying write...");
         j = 0;
         read_flash(address_to_write, verify, 8);
         for (i = 0; i < 8; i++)
         {
                       if (verify[i] != test_pattern[i])
                               j = 1;
                       
                       print_hex_unsigned(verify[i]);
                       print_string(" ");
         }
         if (j)
               print_string("...failed\r\n");
         else
               print_string("...successful\r\n");
         reset_flash();
 }
 */
 print_string("Erasing sectors 0x0 to 0x1ff...\r\n");
 erase_sector_flash(0, 511);
 
 /*for (c = 0; c < 512; c++)
 {
   address_to_write = SECTOR_ADDRESS(c);
   reset_flash();
   print_string("Checking that data was erased in sector ");
       print_hex_unsigned(c);
       print_string("...");
       j = 0;
       read_flash(address_to_write, verify, 16);
       for (i = 0; i < 16; i++)
       {                        
           if (verify[i] != 0xFFFF)
                 j = 1;
                 
           print_hex_unsigned(verify[i]);
               print_string(" ");
       }
       if (j)
         print_string("...failed\r\n");
       else
         print_string("...successful\r\n");
 }
 */
}
#endif


#ifdef TEST_FLASH_WRITE
void test_flash_write(unsigned long addr)
{
	int i, j;
	unsigned short verify[16];
	unsigned short test_pattern[] = { 0x1111, 0x2222, 0x3333, 0x4444, 0x5555, 0x6666, 0x7777, 0x8888, 0x7777, 0x6666, 0x5555, 0x4444, 0x3333, 0x2222, 0x1111, 0x0000 };
	//unsigned short test_pattern[] = { 0x11112222, 0x33334444, 0x55556666, 0x77778888, 0x99998888, 0x77776666, 0x55554444, 0x33332222};

	/*print_string("Writing single word2flash...");
	if (0 != single_word_program_flash(0x60, 0x1234))
          print_string("failed\r\n");
	else
	  print_string("successful\r\n");
	
	reset_flash();
	
	print_string("Verifying write...");
	unsigned short src =  *( (unsigned short *) (FLASH_BASE + ALIGN(addr)));
	print_hex_unsigned(src);
	if (src != 0x1234)
		 print_string("...failed\r\n");
	else
		 print_string("...successful\r\n");
	*/
        //reset_flash();
        int z;
        DELAY(20);
	
	print_string("Writing buffer2flash...");
        print_hex_unsigned(addr);
	if (0 != write_flash(addr, test_pattern, 16))
	  print_string("failed\r\n");
	else
	  print_string("successful\r\n");
	
	//reset_flash();
	
	/*print_string("Verifying write...");
	j = 0;
	read_flash(addr, verify, 16);
	for (i = 0; i < 16; i++)
	{
		if (verify[i] != test_pattern[i])
			j = 1;
		
		print_hex_unsigned(verify[i]);
		print_string(":");
	}
	if (j)
		 print_string("...failed\r\n");
	else
		 print_string("...successful\r\n");*/
	//reset_flash();
}
#endif


#ifdef USE_PRINT_FLASH
// Do not run this method from within the Flash memory 
void print_flash_info()
{
  reset_flash();
  write_flash_unlock_sequence();
  write_flash_command((unsigned char) FLASH_CMD_AUTOSELECT);
  
  unsigned short manuf_id = FLASH_EXTRACT(0x00);
  print_string("Flash Device Information (AUTOSELECT)\r\n");
  print_string("Manufacturer ID: ");
  print_hex_unsigned(manuf_id);
  
  if ((manuf_id) == 0x01)
	  print_string(" (Spansion)\r\n");
  else
	  print_string(" (Unknown)\r\n");
  
  print_string("Device ID: ");
  
  //unsigned short dev_id_1 = FLASH_EXTRACT(0x01);
  unsigned short dev_id_2 = FLASH_EXTRACT(0x0E);
  //unsigned short dev_id_3 = FLASH_EXTRACT(0x0F);
  
  switch (dev_id_2)
  {
    case 0x28:
    	print_string(" S29GL01GP - 1 Gb\r\n");
    	break;
    case 0x23:
    	print_string(" S29GL512P - 512 Mb\r\n");
    	break;
    case 0x22:
    	print_string(" S29GL256P - 256 Mb\r\n");
    	break;
    case 0x21:
    	print_string(" S29GL128P - 128 Mb\r\n");
    	break;
    default:
    	print_string(" Unknown\r\n");
    	break;
  }

  // Reset flash 
  reset_flash();
}
#endif

