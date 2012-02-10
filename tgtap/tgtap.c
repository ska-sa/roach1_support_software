/* ten gig ethernet userspace driver. Warning: Ugly hack ahead */

/* at the moment, only announces itself once, but answers requests */
/* in future should actually issue arp requests, question is simply how often */

/* linux kernels can do arp gleaning, might be useful to enable here */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/utsname.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "tapper.h"

#define GO_MAC          0x00
#define GO_GATEWAY      0x0c
#define GO_ADDRESS      0x10
#define GO_SIZETX       0x18
#define GO_SIZERX       0x1a
#define GO_ENABLE       0x1f
#define GO_PORT         0x20

#define MAX_FRAME       4096
#define ARP_FRAME         64
#define MIN_FRAME         60

#define NAME_BUFFER       64
#define NET_BUFFER     10000
#define IP_BUFFER         16
#define MAC_BUFFER        18
#define CMD_BUFFER      1024

#define GO_TXBUFFER   0x1000
#define GO_RXBUFFER   0x2000

#define GO_ARPTABLE   0x3000
#define ARP_CACHE        256

#define FRAME_TYPE1       12
#define FRAME_TYPE2       13

#define FRAME_DST          0
#define FRAME_SRC          6

#define ARP_OP1            6
#define ARP_OP2            7

#define ARP_SHA_BASE       8
#define ARP_SIP_BASE      14
#define ARP_THA_BASE      18
#define ARP_TIP_BASE      24

#define SIZE_FRAME_HEADER 14

#define IP_DEST1          16
#define IP_DEST2          17
#define IP_DEST3          18
#define IP_DEST4          19

#define POLL_INTERVAL 100000

#define RUNT_LENGTH       20 /* want a basic ip packet */

static const uint8_t arp_const[] = { 0, 1, 8, 0, 6, 4, 0 }; /* disgusting */
static const uint8_t broadcast_const[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

struct getap_state{

  char s_borph_name[NAME_BUFFER];
  char s_tap_name[NAME_BUFFER];

  char s_address_name[IP_BUFFER];
  char s_gateway_name[IP_BUFFER];
  char s_mac_name[MAC_BUFFER];
  unsigned short s_port;

  unsigned int s_index;

  uint8_t s_mac_binary[6];
  uint32_t s_address_binary;
  uint32_t s_mask_binary;
  uint32_t s_network_binary;

  int s_bfd;
  int s_tfd;
  fd_set s_fsr;
  fd_set s_fsw;
  struct timeval s_timeout;

  int s_verbose;
  int s_testing;

  unsigned int s_rxlen;
  unsigned int s_txlen;

  unsigned char s_rxb[MAX_FRAME];
  unsigned char s_txb[MAX_FRAME];

  unsigned char s_arb[ARP_FRAME]; /* arp buffer. sorry */

  uint8_t s_arp[ARP_CACHE][6];
};

/*****************************************************************************/

volatile int run;
static int send_borph(struct getap_state *gs, unsigned char *data, unsigned int len);

/*****************************************************************************/

void make_mac(struct getap_state *gs)
{
  struct utsname un;
  char tmp[12];
  pid_t pid;
  int i, j;

  if(uname(&un) < 0){
    fprintf(stderr, "mac: unable to establish system name\n");
    return;
  }

  memset(tmp, 0, 12);
  if(un.nodename){
    strncpy(tmp, un.nodename, 11);
#ifdef DEBUG
    fprintf(stderr, "mac: my node name is %s\n", un.nodename);
#endif
  } else {
    pid = getpid();
    memcpy(tmp, &pid, 4);
  }

  j = 2;
  strcpy(gs->s_mac_name, "02");

  for(i = 0; i < 5; i++){
    snprintf(gs->s_mac_name + j, MAC_BUFFER - j, ":%x", tmp[i]);
    j += 3;
  }
}

int atomac(unsigned char *binary, const char *text)
{
  int i;
  unsigned int v;
  char *end;
  const char *ptr;

  ptr = text;
  for(i = 0; i < 6; i++){
    v = strtoul(ptr, &end, 16);
    if(v >= 256){
      return -1;
    }
    binary[i] = v;
    if(i < 5){
      if(*end != ':'){
        return -1;
      }
      ptr = end + 1;
    }
  }

#ifdef DEBUG
  fprintf(stderr, "atomac: in %s\natomac: out", text);
  for(i = 0; i < 6; i++){
    fprintf(stderr, " %02x", binary[i]);
  }
  fprintf(stderr, "\n");
#endif

  return 0;
}

int write_mac_borph(struct getap_state *gs, unsigned int offset, const uint8_t *mac)
{
  uint32_t v[2];

  if(gs->s_verbose > 1){
    fprintf(stderr, "write: [0x%x] <- %x:%x:%x:%x:%x:%x\n", offset, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  }

  /* could be done using pwrite too */
  if(lseek(gs->s_bfd, offset, SEEK_SET) != offset){
    fprintf(stderr, "write: unable to seek to 0x%x\n", offset);
    return -1;
  }

  v[0] = ((mac[0] <<  8) & 0xff00) | 
          (mac[1]        & 0xff);
  v[1] = ((mac[2] << 24) & 0xff000000) | 
         ((mac[3] << 16) & 0xff0000) |
         ((mac[4] <<  8) & 0xff00) |
          (mac[5]        & 0xff);

  if(write(gs->s_bfd, v, 8) != 8){
    return -1;
  }

  return 0;
}

int write_u32_borph(struct getap_state *gs, unsigned int offset, uint32_t value)
{
  /* could be done using pwrite too */

  if(gs->s_verbose > 1){
    fprintf(stderr, "write: [0x%x] <- 0x%08x\n", offset, value);
  }

  if(lseek(gs->s_bfd, offset, SEEK_SET) != offset){
    fprintf(stderr, "write: unable to seek to 0x%x\n", offset);
    return -1;
  }

  if(write(gs->s_bfd, &value, 4) != 4){
    return -1;
  }

  return 0;
}

int write_u16_borph(struct getap_state *gs, unsigned int offset, uint16_t value)
{
  /* could be done using pwrite too */

  if(gs->s_verbose > 1){
    fprintf(stderr, "write: [0x%x] <- 0x%08x\n", offset, value);
  }

  if(lseek(gs->s_bfd, offset, SEEK_SET) != offset){
    fprintf(stderr, "write: unable to seek to 0x%x\n", offset);
    return -1;
  }

  if(write(gs->s_bfd, &value, 2) != 2){
    return -1;
  }

  return 0;
}

int write_u8_borph(struct getap_state *gs, unsigned int offset, uint8_t value)
{
  /* could be done using pwrite too */

  if(gs->s_verbose > 1){
    fprintf(stderr, "write: [0x%x] <- 0x%08x\n", offset, value);
  }

  if(lseek(gs->s_bfd, offset, SEEK_SET) != offset){
    fprintf(stderr, "write: unable to seek to 0x%x\n", offset);
    return -1;
  }

  if(write(gs->s_bfd, &value, 1) != 1){
    return -1;
  }

  return 0;
}

/***************************************************************************/

int set_entry_arp(struct getap_state *gs, unsigned int index, const uint8_t *mac)
{

#ifdef DEBUG
  if(index > ARP_CACHE){
    fprintf(stderr, "arp: logic failure: index %u out of range\n", index);
  }
#endif

  memcpy(gs->s_arp[index], mac, 6);

  return write_mac_borph(gs, GO_ARPTABLE + (8 * index), mac);
}

void glean_arp(struct getap_state *gs, uint8_t *mac, uint8_t *ip)
{
  uint32_t v;

  v = ((ip[0] << 24) & 0xff000000) | 
      ((ip[1] << 16) & 0xff0000) |
      ((ip[2] <<  8) & 0xff00) |
      ( ip[3]        & 0xff);

  if(v == 0){
    return;
  }

  if(ip[3] == 0xff){
    return;
  }

  if((v & gs->s_mask_binary) != gs->s_network_binary){
#ifdef DEBUG
    fprintf(stderr, "glean: not my network 0x%08x != 0x%08x\n", v & gs->s_mask_binary, gs->s_network_binary);
#endif
    return;
  }

#ifdef DEBUG
  fprintf(stderr, "glean: adding entry %d\n", ip[3]);
#endif

  set_entry_arp(gs, ip[3], mac);
}

void announce_arp(struct getap_state *gs)
{
  uint32_t subnet;

  subnet = (~(gs->s_mask_binary)) | gs->s_address_binary;

  memcpy(gs->s_arb + FRAME_DST, broadcast_const, 6);
  memcpy(gs->s_arb + FRAME_SRC, gs->s_mac_binary, 6);

  gs->s_arb[FRAME_TYPE1] = 0x08;
  gs->s_arb[FRAME_TYPE2] = 0x06;

  memcpy(gs->s_arb + SIZE_FRAME_HEADER, arp_const, 7);

  gs->s_arb[SIZE_FRAME_HEADER + ARP_OP2] = 2;

  /* spam the subnet */
  memcpy(gs->s_arb + SIZE_FRAME_HEADER + ARP_TIP_BASE, &subnet, 4);
  memcpy(gs->s_arb + SIZE_FRAME_HEADER + ARP_THA_BASE, broadcast_const, 6);

  /* write in our own sending information */
  memcpy(gs->s_arb + SIZE_FRAME_HEADER + ARP_SIP_BASE, &(gs->s_address_binary), 4);
  memcpy(gs->s_arb + SIZE_FRAME_HEADER + ARP_SHA_BASE, gs->s_mac_binary, 6);

#ifdef DEBUG
  fprintf(stderr, "arp: sending arp announce\n");
#endif

  send_borph(gs, gs->s_arb, 42);
}

static void request_arp(struct getap_state *gs, int index)
{
  uint32_t host;

  if(gs->s_index == index){
    return;
  }

  host = htonl(index) | (gs->s_mask_binary & gs->s_address_binary);

  memcpy(gs->s_arb + FRAME_DST, broadcast_const, 6);
  memcpy(gs->s_arb + FRAME_SRC, gs->s_mac_binary, 6);

  gs->s_arb[FRAME_TYPE1] = 0x08;
  gs->s_arb[FRAME_TYPE2] = 0x06;

  memcpy(gs->s_arb + SIZE_FRAME_HEADER, arp_const, 7);

  gs->s_arb[SIZE_FRAME_HEADER + ARP_OP2] = 1;

  memcpy(gs->s_arb + SIZE_FRAME_HEADER + ARP_TIP_BASE, &host, 4);
  memcpy(gs->s_arb + SIZE_FRAME_HEADER + ARP_THA_BASE, broadcast_const, 6);

  /* write in our own sending information */
  memcpy(gs->s_arb + SIZE_FRAME_HEADER + ARP_SIP_BASE, &(gs->s_address_binary), 4);
  memcpy(gs->s_arb + SIZE_FRAME_HEADER + ARP_SHA_BASE, gs->s_mac_binary, 6);

#ifdef DEBUG
  fprintf(stderr, "arp: sending arp request for index %d (host=0x%08x)\n", index, host);
#endif

  send_borph(gs, gs->s_arb, 42);
}

void reply_arp(struct getap_state *gs)
{
  /* don't use arp buffer arb, just turn the rx buffer around */
  memcpy(gs->s_rxb + FRAME_DST, gs->s_rxb + FRAME_SRC, 6);
  memcpy(gs->s_rxb + FRAME_SRC, gs->s_mac_binary, 6);

  gs->s_rxb[SIZE_FRAME_HEADER + ARP_OP2] = 2;

  /* make sender of receive the target of transmit*/
  memcpy(gs->s_rxb + SIZE_FRAME_HEADER + ARP_THA_BASE, gs->s_rxb + SIZE_FRAME_HEADER + ARP_SHA_BASE, 10); 

  /* write in our own sending information */
  memcpy(gs->s_rxb + SIZE_FRAME_HEADER + ARP_SIP_BASE, &(gs->s_address_binary), 4);
  memcpy(gs->s_rxb + SIZE_FRAME_HEADER + ARP_SHA_BASE, gs->s_mac_binary, 6);

#ifdef DEBUG
  fprintf(stderr, "arp: sending arp reply\n");
#endif

  send_borph(gs, gs->s_rxb, 42);
}

void receive_arp(struct getap_state *gs)
{
#ifdef DEBUG
  fprintf(stderr, "arp: got arp packet\n");
#endif

  if(memcmp(arp_const, gs->s_rxb + SIZE_FRAME_HEADER, 7)){
    fprintf(stderr, "arp: unknown or malformed arp packet\n");
    return;
  }

  switch(gs->s_rxb[SIZE_FRAME_HEADER + ARP_OP2]){
    case 2 : /* reply */
#ifdef DEBUG
      fprintf(stderr, "arp: saw reply\n");
#endif
      glean_arp(gs, gs->s_rxb + SIZE_FRAME_HEADER + ARP_SHA_BASE, gs->s_rxb + SIZE_FRAME_HEADER + ARP_SIP_BASE);
      break;

    case 1 : /* request */
#ifdef DEBUG
      fprintf(stderr, "arp: saw request\n");
#endif
      glean_arp(gs, gs->s_rxb + SIZE_FRAME_HEADER + ARP_SHA_BASE, gs->s_rxb + SIZE_FRAME_HEADER + ARP_SIP_BASE);
      if(!memcmp(gs->s_rxb + SIZE_FRAME_HEADER + ARP_TIP_BASE, &(gs->s_address_binary), 4)){
#ifdef DEBUG
        fprintf(stderr, "arp: somebody is looking for me\n");
#endif
        reply_arp(gs);
      }
      break;
    default :
      fprintf(stderr, "arp: unhandled arp message %x\n", gs->s_rxb[SIZE_FRAME_HEADER + ARP_OP2]);
      break;
  }
}

/***************************************************************************/

int receive_borph(struct getap_state *gs)
{
  /* 1 - useful data, 0 - false alarm, -1 problem */
  uint16_t rx;
  int rr, result, len;
#ifdef DEBUG
  int i;
#endif

  if(gs->s_rxlen > 0){
    fprintf(stderr, "rxb: receive buffer (%u) not yet cleared\n", gs->s_rxlen);
    return -1;
  }

  if(lseek(gs->s_bfd, GO_SIZERX, SEEK_SET) != GO_SIZERX){
    fprintf(stderr, "rxb: unable to seek to 0x%x\n", GO_SIZERX);
    return -1;
  }
  if(read(gs->s_bfd, &rx, 2) != 2){
    fprintf(stderr, "rxb: unable to read flags\n");
    return -1;
  }

  if(rx <= 0){
    return 0;
  }

  len = rx * 8;

#ifdef DEBUG
  fprintf(stderr, "rxb: %d bytes to read\n", len);
#endif

  result = (-1);

  if((len > SIZE_FRAME_HEADER) && (len < MAX_FRAME)){
    if(lseek(gs->s_bfd, GO_RXBUFFER, SEEK_SET) != GO_RXBUFFER){
      fprintf(stderr, "rxb: unable to seek to 0x%x\n", GO_RXBUFFER);
      return -1;
    }

    rr = read(gs->s_bfd, gs->s_rxb, len);
    if(rr < len){
      if(rr < 0){
        fprintf(stderr, "rxb: read failed: %s\n", strerror(errno));
      } else {
        fprintf(stderr, "rxb: short read: %d/%u\n", rr, len);
      }
    } else {
      if(gs->s_verbose > 2){
        fprintf(stderr, "rxb: borph read %d\n", rr);
      }
#ifdef DEBUG
      fprintf(stderr, "rxb: data:");
      for(i = 0; i < rr; i++){
        fprintf(stderr, " %02x", gs->s_rxb[i]);
      }
      fprintf(stderr, "\n");
#endif
      if(gs->s_rxb[FRAME_TYPE1] == 0x08){
        switch(gs->s_rxb[FRAME_TYPE2]){
          case 0x00 : /* IP packet */
            result = 1;
            gs->s_rxlen = rr;
            break;
          case 0x06 : /* arp packet */
            gs->s_rxlen = rr;
            receive_arp(gs);
            gs->s_rxlen = 0;
            result = 0;
            break;
          default :
            fprintf(stderr, "rxb: discarding unknown type 0x%02x%02x\n", gs->s_rxb[FRAME_TYPE1], gs->s_rxb[FRAME_TYPE2]);
            result = 0;
            break;
        }
      } 
    }
  } else {
    fprintf(stderr, "rxb: broken frame, len=%u\n", rx);
    return -1;
  }

  if(lseek(gs->s_bfd, GO_SIZERX, SEEK_SET) != GO_SIZERX){
    fprintf(stderr, "rxb: unable to seek to 0x%x\n", GO_SIZERX);
    return -1;
  }
  rx = 0;
  if(write(gs->s_bfd, &rx, 2) != 2){
    fprintf(stderr, "rxb: unable to write flags\n");
    return -1;
  }

  return result;
}

static int send_borph(struct getap_state *gs, unsigned char *data, unsigned int len)
{
  uint16_t tx;
  int wr;

  if(gs->s_verbose > 1){
    fprintf(stderr, "txb: packet %u\n", len);
  }

  if(len < MIN_FRAME){
    if(len <= 0){
#ifdef DEBUG
      fprintf(stderr, "txb: empty frame, nothing to do\n");
#endif
      return 0;
    } else {
      memset(data + len, 0, MIN_FRAME - len);
      len = MIN_FRAME;
    }
  }

  if(len > MAX_FRAME){
#ifdef DEBUG
    fprintf(stderr, "txb: frame too long\n");
#endif
    return -1;
  }

  if(lseek(gs->s_bfd, GO_SIZETX, SEEK_SET) != GO_SIZETX){
    fprintf(stderr, "txb: unable to seek to 0x%x\n", GO_SIZETX);
    return -1;
  }
  if(read(gs->s_bfd, &tx, 2) != 2){
    fprintf(stderr, "txb: unable to read tx flags\n");
    return -1;
  }
  
  if(tx > 0){
    fprintf(stderr, "txb: tx still busy (%d)\n", tx);
    return -1;
  }

  if(lseek(gs->s_bfd, GO_TXBUFFER, SEEK_SET) != GO_TXBUFFER){
    fprintf(stderr, "txb: unable to seek to 0x%x\n", GO_TXBUFFER);
    return -1;
  }
  wr = write(gs->s_bfd, data, len);
  if(wr < len){
    fprintf(stderr, "txb: write failed: %d < %u\n", wr, len);
    return -1;
  }

  if(lseek(gs->s_bfd, GO_SIZETX, SEEK_SET) != GO_SIZETX){
    fprintf(stderr, "txb: unable to seek to 0x%x\n", GO_SIZETX);
    return -1;
  }
  tx = (len + 7) / 8;
  if(write(gs->s_bfd, &tx, 2) != 2){
    fprintf(stderr, "txb: unable to write tx length\n");
    return -1;
  }

  return 1;
}

int transmit_borph(struct getap_state *gs)
{
  uint8_t *mac;

  
  mac = gs->s_arp[gs->s_txb[SIZE_FRAME_HEADER + IP_DEST4]];
#ifdef DEBUG
  fprintf(stderr, "txb: dst mac %x:%x:%x:%x:%x:%x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
#endif
  memcpy(gs->s_txb, mac, 6);

  if(send_borph(gs, gs->s_txb, gs->s_txlen) < 0){
    return -1;
  }

  gs->s_txlen = 0;

  return 1;
}

int transmit_tap(struct getap_state *gs)
{
  int wr;

  if(gs->s_rxlen <= SIZE_FRAME_HEADER){
    gs->s_rxlen = 0;
    return 0;
  }

  wr = write(gs->s_tfd, gs->s_rxb + SIZE_FRAME_HEADER, gs->s_rxlen - SIZE_FRAME_HEADER);
  if(wr < 0){
    switch(errno){
      case EINTR  :
      case EAGAIN :
        return 0;
      default :
        fprintf(stderr, "txt: write failed: %s\n", strerror(errno));
        return -1;
    }
  }

  gs->s_rxlen = 0;

  if((wr + SIZE_FRAME_HEADER) < gs->s_rxlen){
    fprintf(stderr, "txt: short write %d + %d < %u\n", SIZE_FRAME_HEADER, wr, gs->s_rxlen);
    return -1;
  }

  return 1;
}

int receive_tap(struct getap_state *gs)
{
  int rr;
#ifdef DEBUG
  unsigned int i;
#endif

  if(gs->s_txlen > 0){
#ifdef DEBUG
    fprintf(stderr, "rxt: receive buffer still in use (%d)\n", gs->s_txlen);
#endif
    return 0;
  }

  rr = read(gs->s_tfd, gs->s_txb + SIZE_FRAME_HEADER, MAX_FRAME - SIZE_FRAME_HEADER);
  if(rr < 0){
    switch(errno){
      case EAGAIN :
      case EINTR : 
        return 0;
      default :
        fprintf(stderr, "rxt: unable to read from tap: %s\n", strerror(errno));
        return -1;
    }
  }

  if(rr < RUNT_LENGTH){
    fprintf(stderr, "rxt: read problems: length=%d\n", gs->s_txlen);
    return -1;
  }

#ifdef DEBUG
  fprintf(stderr, "rxt: tap rx=%d, data=", rr);
  for(i = 0; i < rr; i++){
    fprintf(stderr, " %02x", gs->s_txb[i]); 
  }
  fprintf(stderr, "\n");
#endif

  gs->s_txlen = rr + 14;

  return 1;
}

/****************************************************************************/

int setup_borph(struct getap_state *gs)
{
  struct in_addr in;
  uint32_t v;
  int flags;
  unsigned int i;

  if(gs->s_verbose > 1){
    fprintf(stderr, "setup: opening register file %s\n", gs->s_borph_name);
  }

  flags = O_RDWR;
  if(gs->s_testing){
    flags |= O_CREAT | O_TRUNC;
  }

  gs->s_bfd = open(gs->s_borph_name, flags, S_IRUSR | S_IWUSR);
  if(gs->s_bfd < 0){
    fprintf(stderr, "setup: unable to access %s register file %s: %s\n", gs->s_testing  ? "testing" : "borph", gs->s_borph_name, strerror(errno));
    return -1;
  }

  if(atomac(gs->s_mac_binary, gs->s_mac_name) < 0){
    fprintf(stderr, "setup: unable to convert %s to mac\n", gs->s_mac_name);
    return -1;
  }
  if(write_mac_borph(gs, GO_MAC, gs->s_mac_binary) < 0){
    fprintf(stderr, "setup: unable to set own mac\n");
    return -1;
  }

  memcpy(gs->s_txb + 6, gs->s_mac_binary, 6);
  gs->s_txb[FRAME_TYPE1] = 0x08;
  gs->s_txb[FRAME_TYPE2] = 0x00;

  if(gs->s_gateway_name[0] != '\0'){
    if(inet_aton(gs->s_gateway_name, &in) == 0){
      fprintf(stderr, "setup: unable to convert <%s> to ip address\n", gs->s_gateway_name);
      return -1;
    }
    v = (in.s_addr) & 0xff;
    if(write_u32_borph(gs, GO_GATEWAY, v)){
      return -1;
    }
  }

  if(inet_aton(gs->s_address_name, &in) == 0){
    fprintf(stderr, "setup: unable to convert <%s> to ip address\n", gs->s_address_name);
    return -1;
  }
  if(sizeof(in.s_addr) != 4){
    fprintf(stderr, "setup: logic problem: ip address not 4 bytes\n");
    return -1;
  }
  v = in.s_addr;

  gs->s_address_binary = v; /* in network byte order */
  gs->s_mask_binary = htonl(0xffffff00); /* fixed mask */
  gs->s_network_binary = gs->s_mask_binary & gs->s_address_binary;

  gs->s_index = ntohl(~(gs->s_mask_binary) & gs->s_address_binary);

  /* assumes plain big endian value */
  if(write_u32_borph(gs, GO_ADDRESS, v)){
    return -1;
  }

  if(gs->s_port){
    /* assumes plain big endian value */
    v = gs->s_port;
    if(write_u16_borph(gs, GO_PORT, v)){
      return -1;
    }
  }

  for(i = 0; i < ARP_CACHE; i++){
    set_entry_arp(gs, i, broadcast_const);
  }

  /* know thyself */
  set_entry_arp(gs, gs->s_index, gs->s_mac_binary);

  if(write_u8_borph(gs, GO_ENABLE, 1)){
    return -1;
  }

  return 0;
}

int setup_tap(struct getap_state *gs)
{
  char cmd_buffer[CMD_BUFFER];
  int len;

  len = snprintf(cmd_buffer, CMD_BUFFER, "ifconfig %s %s netmask 255.255.255.0 up\n", gs->s_tap_name, gs->s_address_name);
  cmd_buffer[CMD_BUFFER - 1] = '\0';

  if(system(cmd_buffer)){
    return -1;
  }

  return 0;
}

/****************************************************************************/

int mainloop(struct getap_state *gs)
{
  int result, max, busy, index;

  FD_ZERO(&(gs->s_fsr));
  FD_ZERO(&(gs->s_fsw));

  max = gs->s_tfd + 1;
  busy = 1;
  index = 1;

  announce_arp(gs);

  while(run){

    if(index < 255){
      if(busy == 0){
        request_arp(gs, index);
        index++;
      }
    }

    FD_SET(gs->s_tfd, &(gs->s_fsr));
    FD_SET(gs->s_tfd, &(gs->s_fsw));

    gs->s_timeout.tv_sec = 0;
    gs->s_timeout.tv_usec = busy ? 0 : POLL_INTERVAL;

    busy = 0;

    result = select(max, &(gs->s_fsr), (gs->s_rxlen > 0) ? &(gs->s_fsw) : NULL, NULL, &(gs->s_timeout));
    if(result < 0){
      switch(errno){
        case EAGAIN :
        case EINTR :
          break;
        default :
          fprintf(stderr, "mainloop: unable to select: %s\n", strerror(errno));
          return -1;
      }
    }

    if(FD_ISSET(gs->s_tfd, &(gs->s_fsr))){
      /* read, if we have space */
      receive_tap(gs);
      busy = 1;
    }

    if(FD_ISSET(gs->s_tfd, &(gs->s_fsw))){
      transmit_tap(gs);
    }

    /* poll borph */
    if(receive_borph(gs) > 0){
      busy = 1;
    }

    /* write txb if there is anything */
    if(gs->s_txlen){
      transmit_borph(gs);
    }

  }

  return 0;
}

void usage(char *app)
{
  printf("usage: %s\n", app);
  printf("-a ip    local ip address\n");
  printf("-g ip    gateway ip address\n");
  printf("-m mac   MAC address\n");
  printf("-p port  fabric port\n");
  printf("-t name  tap device name\n");
  printf("-b name  borph file name\n");
  printf("-T       enable test mode (-b specifies normal file)\n");
}

int main(int argc, char **argv)
{
  int result, index;
  char *bptr, *tptr;
  char *app;
  int i, j, c;
  struct getap_state *gs;

  index = 0;
  app = argv[0];
  i = j = 1;

  tptr = "tgtap%d";
  bptr = "/proc/%d/hw/ioreg/ten_Gbe_v2";

  gs = malloc(sizeof(struct getap_state));
  if(gs == NULL){
    fprintf(stderr, "%s: unable to allocate %d bytes\n", app, sizeof(struct getap_state));
    return EX_USAGE;
  }

  gs->s_address_name[0] = '\0';
  gs->s_gateway_name[0] = '\0';
  gs->s_mac_name[0] = '\0';
  gs->s_port = 0;

  gs->s_verbose = 1;
  gs->s_testing = 0;

  while (i < argc) {
    if (argv[i][0] == '-') {
      c = argv[i][j];
      switch (c) {
        case 'h' :
          usage(app);
          return EX_OK;
        case 'v' :
          gs->s_verbose++;
          j++;
          break;
        case 'T' :
          gs->s_testing++;
          j++;
          break;
        case 'a' :
        case 'g' :
        case 'm' :
        case 'p' :
        case 'n' :
        case 't' : 
        case 'b' : 
          j++;
          if (argv[i][j] == '\0') {
            j = 0;
            i++;
          }
          if (i >= argc) {
            fprintf(stderr, "%s: -%c needs a parameter\n", app, c);
            return EX_USAGE;
          }

          switch(c){
            case 'a' :
              strncpy(gs->s_address_name, argv[i] + j, IP_BUFFER - 1);
              gs->s_address_name[IP_BUFFER - 1] = '\0';
              break;
            case 'g' :
              strncpy(gs->s_gateway_name, argv[i] + j, IP_BUFFER - 1);
              gs->s_gateway_name[IP_BUFFER - 1] = '\0';
              break;
            case 'm' :
              strncpy(gs->s_mac_name, argv[i] + j, MAC_BUFFER - 1);
              gs->s_mac_name[MAC_BUFFER - 1] = '\0';
            case 'p' :
              gs->s_port = atoi(argv[i] + j);
              break;
            case 'n' :
              index = atoi(argv[i] + j);
              break;
            case 't' :
              tptr = NULL;
              strncpy(gs->s_tap_name, argv[i] + j, NAME_BUFFER - 1);
              gs->s_tap_name[NAME_BUFFER - 1] = '\0';
              break;
            case 'b' :
              bptr = NULL;
              strncpy(gs->s_borph_name, argv[i] + j, NAME_BUFFER - 1);
              gs->s_borph_name[NAME_BUFFER - 1] = '\0';
              break;
          }

          i++;
          j = 1;
          break;

        case '\0':
          j = 1;
          i++;
          break;
        default:
          fprintf(stderr, "%s: unknown option -%c\n", app, argv[i][j]);
          return EX_USAGE;
      }
    } else {
      fprintf(stderr, "%s: bad argument %s\n", app, argv[i]);
      return EX_USAGE;
    }
  }

  if(tptr){
    result = snprintf(gs->s_tap_name, NAME_BUFFER - 1, tptr, index); 
    if((result < 0) || (result >= NAME_BUFFER)){
      fprintf(stderr, "unable to expand %s: Value too long\n", tptr);
      return EX_USAGE;
    }
  }
  if(bptr){
    result = snprintf(gs->s_borph_name, NAME_BUFFER - 1, bptr, getpid()); 
    if((result < 0) || (result >= NAME_BUFFER)){
      fprintf(stderr, "unable to expand %s: Value too long\n", bptr);
      return EX_USAGE;
    }
  }
  if(gs->s_address_name[0] == '\0'){
    fprintf(stderr, "%s: need an ip address\n", app);
    return EX_OSERR;
  }

  if(gs->s_gateway_name[0] == '\0'){
    /* risky, gateware may not like it */
    strncpy(gs->s_gateway_name, gs->s_address_name, IP_BUFFER);
  }

  if(gs->s_mac_name[0] == '\0'){
    make_mac(gs);
  }

  if(gs->s_verbose){
    printf("%s: tap interface name: %s\n", app, gs->s_tap_name);
    printf("%s: borph file: %s\n", app, gs->s_borph_name);
    printf("%s: %s file interface \n", app, gs->s_testing ? "testing" : "borph");
    printf("%s: ip address %s\n", app, gs->s_address_name);
    printf("%s: mac address %s\n", app, gs->s_mac_name);
  }

  gs->s_tfd = tap_open(gs->s_tap_name);
  if(gs->s_tfd < 0){
    fprintf(stderr, "%s: unable to set up tap device %s: %s\n", app, gs->s_tap_name, strerror(errno));
    return EX_OSERR;
  }

  if(setup_borph(gs) < 0){
    fprintf(stderr, "%s: unable to initialise borph register file %s: %s\n", app, gs->s_borph_name, strerror(errno));
    return EX_OSERR;
  }

  if(setup_tap(gs)){
    fprintf(stderr, "%s: unable to configure tap device %s\n", app, gs->s_tap_name);
    return EX_OSERR;
  }

  printf("%s: my arp table index %d\n", app, gs->s_index);

  if(gs->s_verbose){
    printf("%s: my network: 0x%08x\n", app, gs->s_network_binary);
  }

  run = 1;

  /* signal stuff */

  mainloop(gs);

  tap_close(gs->s_tfd);
  close(gs->s_bfd);

  return EX_OK;
}
