#ifndef FLASH_H_
#define FLASH_H_

#define FLASH_INFO

/* Spansion S29GL-P Flash driver */
#define S29GLP 1

#define FLASH_BASE           0x60000000
#define FLASH_END            0x64000000
#define FLASH_BUFFER_SIZE    32

#define FLASH_WRITE_TIMEOUT  500
#define FLASH_ERASE_TIMEOUT  120000

/* S29GL01GP - 1Gb flash */
#define FLASH_SECTOR_COUNT   1024
#define FLASH_SECTOR_WIDTH   128

#if 0
int single_word_program_flash(unsigned long address, unsigned short data);
int write_flash(unsigned long dest, unsigned short *src, unsigned short n);
int read_flash(unsigned long address, unsigned short *dest, unsigned short n);
int erase_sector_flash(unsigned int start_sector, unsigned int end_sector);
int erase_chip_flash();
void reset_flash();
void suspend_flash_erase();
void resume_flash_erase(unsigned int sector);
void suspend_flash_write();
void resume_flash_write();
void unlock_bypass_flash();
void unlock_bypass_reset_flash();
void test_flash_write(unsigned long addr);
void test_flash_erase();

#ifdef FLASH_INFO
/* Do not run this method from within the Flash memory */
void print_flash_info();
#endif
#endif

int erase_chip_flash();
int erase_sector_flash(unsigned int);
void reset_flash();
int write_flash(unsigned int dst, unsigned int src, unsigned int n);
void poll_flash();

#endif
