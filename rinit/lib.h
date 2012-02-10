#ifndef LIB_H
#define LIB_H

/* cheat, implemented in serial.S */
void print_char(unsigned int x);
unsigned int test_char();

/* defined in lib.c */
void print_string(char *s);
void print_hex_unsigned(unsigned int x);
void print_newline();
unsigned int get_byte(unsigned int *timeout);
unsigned int get_hex_unsigned();

void flush();
void barrier();

/* defined in xmodem.c */
#define XMODEM_BLOCK          128

struct xmodem{
  unsigned char x_buffer[XMODEM_BLOCK];
  unsigned char x_frame;
  unsigned char x_last;
  unsigned int x_timeout;
  unsigned int x_databytes;
};

void init_xmodem(struct xmodem *xm);
int getblock_xmodem(struct xmodem *xm);

#endif
