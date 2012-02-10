/* xmodem transfer routines, reused from one of my previous projects predating roach */
/* Marc Welz <marc@welz.org.za> */

#include "lib.h"
#include "serial.h"

#define XMODEM_SOH        0x01
#define XMODEM_EOT        0x04
#define XMODEM_ACK        0x06
#define XMODEM_NAK        0x15
#define XMODEM_CAN        0x18


#ifdef RINIT_PROGRAM

#define XMODEM_LARGE_TIMEOUT   10000000
#define XMODEM_MINOR_TIMEOUT    2000000
#define XMODEM_NAK_REPEATS            7
#define XMODEM_JUNK_DISCARD    (XMODEM_BLOCK + 3)
#define XMODEM_GRACE_COUNT            4

static int prepare_xmodem(struct xmodem *xm)
{
  unsigned int c;

  print_char(xm->x_last);
  xm->x_last = XMODEM_NAK;

  for(;;){
    xm->x_timeout = XMODEM_LARGE_TIMEOUT;
    c = get_byte(&(xm->x_timeout));
    switch(c){
      case XMODEM_EOT :
        /* TODO: check if the EOT needs to be acked ? */
        print_char(XMODEM_ACK);
        return 1;
      case XMODEM_SOH :
        return 0;
      case XMODEM_CAN :
        print_char(0x80);
        return -1;
      case SERIAL_TIMEOUT :
        print_char(XMODEM_NAK);
        break;
    }
  }
}

#else

#define XMODEM_LARGE_TIMEOUT   10000000
#define XMODEM_MINOR_TIMEOUT    2000000
#define XMODEM_NAK_REPEATS            7
#define XMODEM_JUNK_DISCARD    (XMODEM_BLOCK + 3)
#define XMODEM_GRACE_COUNT            4

static int prepare_xmodem(struct xmodem *xm)
{
  unsigned int i, c, s;

  xm->x_timeout = XMODEM_LARGE_TIMEOUT;
  print_char(xm->x_last);
  xm->x_last = XMODEM_NAK;

  for(s = 0, i = 0; i < (s ? XMODEM_JUNK_DISCARD : XMODEM_NAK_REPEATS); i++){
    c = get_byte(&(xm->x_timeout));
    switch(c){
      case XMODEM_EOT :
        /* TODO: check if the EOT needs to be acked ? */
        print_char(XMODEM_ACK);
        return 1;
      case XMODEM_SOH :
        return 0;
      case XMODEM_CAN :
        print_char(0x80);
        return -1;
      case SERIAL_TIMEOUT :
        if(s == 0){
          print_char(XMODEM_NAK);
          xm->x_timeout = XMODEM_LARGE_TIMEOUT;
        }
        break;
      default :
        xm->x_timeout = XMODEM_MINOR_TIMEOUT;
        s = 1;
        break;
    }
  }

  print_char(XMODEM_NAK);
  print_char(0x81);
  return -1;
}
#endif

static unsigned int sequence_xmodem(struct xmodem *xm)
{
  unsigned int x, y;

  xm->x_timeout = (2 * XMODEM_MINOR_TIMEOUT);
  x = get_byte(&(xm->x_timeout));
  y = get_byte(&(xm->x_timeout));

  if((255 - (x & 0xff)) != y){
    print_char(XMODEM_NAK);
    print_char(0x82);
    return -1;
  }

  if(x != xm->x_frame){
    print_char(0x83);
    return -1;
  }

  return 0;
}

static int collect_xmodem(struct xmodem *xm)
{
  unsigned int i, c, s, d;

  s = 0;
  i = 0; 
  d = 0;

  while(i < XMODEM_BLOCK){
     xm->x_timeout = XMODEM_MINOR_TIMEOUT;
     c = get_byte(&(xm->x_timeout));
     if(c == SERIAL_TIMEOUT){
       d++;
       if(d > XMODEM_GRACE_COUNT){
         print_char(XMODEM_NAK);
         print_char(0x84);
         return -1;
       }
       continue;
     }
     xm->x_buffer[i] = c;
     s += c;
     xm->x_databytes++;
     i++;
  }

  c = get_byte(&(xm->x_timeout));
  if(c == SERIAL_TIMEOUT){
    print_char(XMODEM_NAK);
    print_char(0x85);
    return -1;
  }

  if((c & 0xff) != (s & 0xff)){
    print_char(XMODEM_NAK);
    print_char(0x86);
    return -1;
  }

  xm->x_last = XMODEM_ACK;
  xm->x_frame++;

  return 0;
}

/***********************************/

void init_xmodem(struct xmodem *xm)
{
  xm->x_frame = 1;
  xm->x_databytes = 0;
  xm->x_last = XMODEM_NAK;
}

/* returns -1 on fail, 0 if more blocks, 1 if last block */

int getblock_xmodem(struct xmodem *xm)
{
  int result;

  result = prepare_xmodem(xm);
  if(result){
    return result;
  }

  if(sequence_xmodem(xm)){
    return -1;
  }

  if(collect_xmodem(xm) < 0){
    return -1;
  }

  return 0;
}
