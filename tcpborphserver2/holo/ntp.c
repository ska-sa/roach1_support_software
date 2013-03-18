#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <arpa/inet.h>

#include <katcp.h>

#include "holo.h"

#ifdef STANDALONE
#define log_message_katcp(...)
#endif

#define NTP_MAGIC                   0x1f113123

#define SET_BITS(v, s, m)  (((v) & (m)) << (s))
#define GET_BITS(v, s, m)  (((v) >> (s)) & (m))

#define NTP_VERSION_SHIFT                  11
#define NTP_VERSION_MASK               0x0007
#define  NTP_VERSION_THREE                  3

#define NTP_MODE_SHIFT                      8 
#define NTP_MODE_MASK                  0x0007
#define  NTP_MODE_CONTROL                   6

#define NTP_RESPONSE                   0x0080
#define NTP_ERROR                      0x0040
#define NTP_MORE                       0x0020 

#define NTP_OPCODE_SHIFT                    0
#define NTP_OPCODE_MASK                0x001f
#define  NTP_OPCODE_STATUS                  1

#define NTP_LEAPI_SHIFT                    14
#define NTP_LEAPI_MASK                 0x0003
#define  NTP_LEAPI_NOWARN              0x0000
#define  NTP_LEAPI_SIXTYONE            0x0001
#define  NTP_LEAPI_FIFTYNINE           0x0002
#define  NTP_LEAPI_ALARM               0x0003

#define NTP_CLOCKSRC_SHIFT                  8
#define NTP_CLOCKSRC_MASK              0x003f
#define  NTP_CLOCKSRC_NTPUDP                6

#define NTP_SYSEVTCNT_SHIFT                 4
#define NTP_SYSEVTCNT_MASK             0x000f

#define NTP_SYSEVTCDE_SHIFT                 0
#define NTP_SYSEVTCDE_MASK             0x000f

#define NTP_PEER_STATUS_SHIFT              11
#define NTP_PEER_STATUS_MASK           0x001f

#define  NTP_PEER_STATUS_CONFIGURED    0x0010
#define  NTP_PEER_STATUS_AUTHENABLE    0x0008
#define  NTP_PEER_STATUS_AUTHENTIC     0x0004
#define  NTP_PEER_STATUS_REACHABLE     0x0002
#define  NTP_PEER_STATUS_RESERVED      0x0001

#define NTP_PEER_SELECT_SHIFT               8
#define NTP_PEER_SELECT_MASK           0x0007

#define  NTP_PEER_SELECT_REJECT             0
#define  NTP_PEER_SELECT_SANE               1
#define  NTP_PEER_SELECT_CORRECT            2
#define  NTP_PEER_SELECT_CANDIDATE          3
#define  NTP_PEER_SELECT_OUTLIER            4
#define  NTP_PEER_SELECT_FAR                5
#define  NTP_PEER_SELECT_CLOSE              6
#define  NTP_PEER_SELECT_RESERVED           7

#define NTP_PEER_EVTCNT_SHIFT               4
#define NTP_PEER_EVTCNT_MASK           0x000f

#define NTP_PEER_EVTCDE_SHIFT               0
#define NTP_PEER_EVTCDE_MASK           0x000f

#define NTP_MAX_DATA                      468
#define NTP_HEADER                         12

#define NTP_MAX_PACKET    (NTP_HEADER + NTP_MAX_DATA)

struct ntp_peer_poco{
  uint16_t p_id;
  uint16_t p_status;
} __attribute__ ((packed));

struct ntp_message_poco{
  uint16_t n_bits;
  uint16_t n_sequence;
  uint16_t n_status;
  uint16_t n_id;
  uint16_t n_offset;
  uint16_t n_count;
  uint8_t n_data[NTP_MAX_DATA];
} __attribute__ ((packed));

/*****************************************************************************/

int send_ntp_poco(struct katcp_dispatch *d, struct ntp_sensor_poco *nt)
{
  struct ntp_message_poco buffer, *nm;
  int sw, wr;

  nm = &buffer;

  nm->n_bits = htons(
    SET_BITS(NTP_VERSION_THREE, NTP_VERSION_SHIFT, NTP_VERSION_MASK) | 
    SET_BITS(NTP_MODE_CONTROL,  NTP_MODE_SHIFT,    NTP_MODE_MASK) | 
    SET_BITS(NTP_OPCODE_STATUS, NTP_OPCODE_SHIFT,  NTP_OPCODE_MASK));

  nt->n_sequence = 0xffff & (nt->n_sequence + 1);

  nm->n_sequence = htons(nt->n_sequence);
  nm->n_status   = htons(0);
  nm->n_id       = htons(0);
  nm->n_offset   = htons(0);
  nm->n_count    = htons(0);

  sw = NTP_HEADER;

  wr = send(nt->n_fd, nm, sw, MSG_NOSIGNAL);

  if(wr < sw){
    if(wr < 0){
      switch(errno){
        case EAGAIN :
        case EINTR  :
          return 0;
        default :
          break;
      }
    }

    return -1;
  }

  return 1;
}

int recv_ntp_poco(struct katcp_dispatch *d, struct ntp_sensor_poco *nt)
{
  struct ntp_message_poco buffer, *nm;
  struct ntp_peer_poco *np;
  int rr, i, good;
  uint16_t word, field;

  nm = &buffer;

  rr = recv(nt->n_fd, nm, NTP_MAX_PACKET, MSG_DONTWAIT);
  if(rr < 0){
    switch(errno){
      case EAGAIN :
      case EINTR  :
        return 0;
      default :
        log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "ntp receive failed with %s", strerror(errno));
        return -1;
    }
  }

  if(rr < NTP_HEADER){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "ntp reply of %d bytes too short", rr);
    return -1;
  }

  nm->n_bits     = ntohs(nm->n_bits);
  nm->n_sequence = ntohs(nm->n_sequence);
  nm->n_status   = ntohs(nm->n_status);
  nm->n_id       = ntohs(nm->n_id);
  nm->n_offset   = ntohs(nm->n_offset);
  nm->n_count    = ntohs(nm->n_count);

  field = GET_BITS(nm->n_bits, NTP_VERSION_SHIFT, NTP_VERSION_MASK);
  if(field != NTP_VERSION_THREE){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "ntp reply %d not version 3", field);
    return -1;
  }

  field = GET_BITS(nm->n_bits, NTP_MODE_SHIFT, NTP_MODE_MASK);
  if(field != NTP_MODE_CONTROL){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "ntp message not control but %d", field);
    return -1;
  }

  field = GET_BITS(nm->n_bits, NTP_OPCODE_SHIFT, NTP_OPCODE_MASK);
  if(field != NTP_OPCODE_STATUS){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "ntp opcode not status but %d", field);
    return -1;
  }

  if(!(nm->n_bits & NTP_RESPONSE)){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "ntp reply is not a response");
    return -1;
  }

  if(nm->n_bits & NTP_ERROR){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "ntp sent an error response");
    return -1;
  }

  if(nm->n_sequence != nt->n_sequence){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "ntp received sequence number %d not %d", nm->n_sequence, nt->n_sequence);
    return -1;
  }

#ifdef DEBUG
  fprintf(stderr, "status is 0x%04x\n", nm->n_status);
#endif

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "ntp status word is 0x%04x", nm->n_status);

  field = GET_BITS(nm->n_status, NTP_LEAPI_SHIFT, NTP_LEAPI_MASK);

  switch(field){
    case NTP_LEAPI_NOWARN :
    case NTP_LEAPI_SIXTYONE :
    case NTP_LEAPI_FIFTYNINE :
      break;
    default :
      log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "ntp reports error status");
      return -1;
  }

  field = GET_BITS(nm->n_status, NTP_CLOCKSRC_SHIFT, NTP_CLOCKSRC_MASK);
  if(field != NTP_CLOCKSRC_NTPUDP){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "ntp synchronised by unsual means %d", field);
  }

  if(nm->n_count % 4) {
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "ntp packet has odd length of %d", nm->n_count);
    return -1;
  }

  if((nm->n_offset + nm->n_count) > NTP_MAX_DATA){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "ntp packet of %d and %d exceeds size limits", nm->n_offset, nm->n_count);
    return -1;
  }

  good = 0;

  for(i = 0; i < nm->n_count; i += 4){
    np = (struct ntp_peer_poco *)(nm->n_data + i);

    word = ntohs(np->p_status);

    log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "ntp peer %d reports status 0x%x", ntohs(np->p_id), word);

    field = GET_BITS(word, NTP_PEER_STATUS_SHIFT, NTP_PEER_STATUS_MASK);
    if((field & (NTP_PEER_STATUS_CONFIGURED | NTP_PEER_STATUS_REACHABLE)) == (NTP_PEER_STATUS_CONFIGURED | NTP_PEER_STATUS_REACHABLE)){

      field = GET_BITS(word, NTP_PEER_SELECT_SHIFT, NTP_PEER_SELECT_MASK);
      switch(field){
        case NTP_PEER_SELECT_FAR : 
        case NTP_PEER_SELECT_CLOSE : 
          good = 1;
          break;
        default :
          break;
      }

    } else {
      log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "ntp peer %d either unreachable or unconfigured", ntohs(np->p_id));
    }
  }

  return good;
}

/*****************************************************************************/

int acquire_ntp_poco(struct katcp_dispatch *d, void *data)
{
  struct ntp_sensor_poco *nt;
  int result;

  nt = data;

  if((nt == NULL) || (nt->n_magic != NTP_MAGIC)){
    log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "null or bad magic on ntp state while doing acquire");
    return -1;
  }

  result = 1;
  if(recv_ntp_poco(d, nt) <= 0){
    result = 0;
  }

  if(send_ntp_poco(d, nt) < 0){
    result = 0;
  }

  return result;
}

void clear_ntp_poco(struct katcp_dispatch *d, struct ntp_sensor_poco *nt)
{

  if(nt->n_magic != NTP_MAGIC){
    log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "bad magic on ntp state");
  }

  if(nt->n_fd >= 0){
    close(nt->n_fd);
    nt->n_fd = (-1);
  }
}

int init_ntp_poco(struct katcp_dispatch *d, struct ntp_sensor_poco *nt)
{
  int fd;
  struct sockaddr_in sa;

  nt->n_magic = NTP_MAGIC;

  if(nt->n_fd >= 0){
    log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "closing previous ntp file descriptor");
    close(nt->n_fd);
    nt->n_fd = (-1);
  }

  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  sa.sin_port = htons(123);

  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if(fd < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to create ntp socket: %s", strerror(errno));
    return -1;
  }

  if(connect(fd, (struct sockaddr *)&sa, sizeof(struct sockaddr_in)) < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to connect to ntp: %s", strerror(errno));
    close(fd);
    fd = (-1);
    return -1;
  }

  nt->n_fd = fd;
  nt->n_sequence = 1;

  send_ntp_poco(d, nt);

  return 0;
}

#ifdef STANDALONE
int main()
{
  struct ntp_sensor_poco ntp_sensor, *nt;

  nt = &ntp_sensor;

  if(init_ntp_poco(NULL, nt) < 0){
    fprintf(stderr, "unable to init sensor\n");
    return 1;
  }

  if(send_ntp_poco(NULL, nt) < 0){
    fprintf(stderr, "unable to send request\n");
    return 2;
  }

  sleep(1);

  if(recv_ntp_poco(NULL, nt) <= 0){
    fprintf(stderr, "unable to receive request\n");
    return 3;
  }

  return 0;
}
#endif

#if 0
int recv_ntp_poco(struct katcp_dispatch *d, struct ntp_sensor_poco *nt)
{
  struct ntp_message_poco buffer, *nm;
  struct ntp_peer_poco *np;
  int rr, i;
  unsigned int bytes, offset;
  uint16_t word, field;

  nm = &buffer;

  rr = recv(nt->n_fd, nm, NTP_MAX_PACKET, MSG_DONTWAIT);
  if(rr < 0){
    switch(errno){
      case EAGAIN :
      case EINTR  :
        return 0;
      default :
        return -1;
    }
  }

  if(rr < NTP_HEADER){
    return -1;
  }

  nm->n_bits     = ntohs(nm->n_bits);
  nm->n_sequence = ntohs(nm->n_sequence);
  nm->n_status   = ntohs(nm->n_status);
  nm->n_id       = ntohs(nm->n_id);
  nm->n_offset   = ntohs(nm->n_offset);
  nm->n_count    = ntohs(nm->n_count);

  field = GET_BITS(nm->n_bits, NTP_VERSION_SHIFT, NTP_VERSION_MASK);
  if(field != NTP_VERSION_THREE){
    fprintf(stderr, "not a version three message but %d\n", field);
    return -1;
  }

  field = GET_BITS(nm->n_bits, NTP_MODE_SHIFT, NTP_MODE_MASK);
  if(field != NTP_MODE_CONTROL){
    fprintf(stderr, "message not control but %d\n", field);
    return -1;
  }

  field = GET_BITS(nm->n_bits, NTP_OPCODE_SHIFT, NTP_OPCODE_MASK);
  if(field != NTP_OPCODE_STATUS){
    fprintf(stderr, "opcode not status but but %d\n", field);
    return -1;
  }

  if(!(nm->n_bits & NTP_RESPONSE)){
#if 0
    log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "received ntp message which is not a response");
#endif
    fprintf(stderr, "received a message which isn't a response\n");
    return -1;
  }

  if(nm->n_bits & NTP_ERROR){
    fprintf(stderr, "got an error response\n");
    /* TODO ? */
  }

  if(nm->n_sequence != nt->n_sequence){
    fprintf(stderr, "bad sequence number, expected %d, got %d\n", nt->n_sequence, nm->n_sequence);
    return -1;
  }

#ifdef DEBUG
  fprintf(stderr, "status is 0x%04x\n", nm->n_status);
#endif

  field = GET_BITS(nm->n_status, NTP_LEAPI_SHIFT, NTP_LEAPI_MASK);
  fprintf(stderr, "leap indicator is 0x%x\n", field);

  field = GET_BITS(nm->n_status, NTP_CLOCKSRC_SHIFT, NTP_CLOCKSRC_MASK);
  fprintf(stderr, "clock source is 0x%x\n", field);

  field = GET_BITS(nm->n_status, NTP_SYSEVTCNT_SHIFT, NTP_SYSEVTCNT_MASK);
  fprintf(stderr, "got %d new system events\n", field);

  field = GET_BITS(nm->n_status, NTP_SYSEVTCDE_SHIFT, NTP_SYSEVTCDE_MASK);
  fprintf(stderr, "most recent code 0x%x ", field);

#ifdef DEBUG
  fprintf(stderr, "got a further %u data bytes starting at %u\n", nm->n_count, nm->n_offset);
#endif

  if(nm->n_count % 4) {
    fprintf(stderr, "logic problem - packet data size not a multiple of 4\n");
    return -1;
  }

  if((nm->n_offset + nm->n_count) > NTP_MAX_DATA){
    fprintf(stderr, "logic problem - data field %u+%u too large\n", nm->n_offset, nm->n_count);
    return -1;
  }

  for(i = 0; i < nm->n_count; i += 4){
    np = (struct ntp_peer_poco *)(nm->n_data + i);

#ifdef DEBUG
    fprintf(stderr, "peer id 0x%04x/%u\n", ntohs(np->p_id), ntohs(np->p_id));
#endif

    word = ntohs(np->p_status);

    field = GET_BITS(word, NTP_PEER_STATUS_SHIFT, NTP_PEER_STATUS_MASK);

#ifdef DEBUG
    fprintf(stderr, " field=0x%04x", word);
#endif

#ifdef DEBUG
    if(field & NTP_PEER_STATUS_CONFIGURED){
      fprintf(stderr, " configured");
    }
#endif

#ifdef DEBUG
    if(field & NTP_PEER_STATUS_REACHABLE){
      fprintf(stderr, " reachable");
    }
#endif

    field = GET_BITS(word, NTP_PEER_SELECT_SHIFT, NTP_PEER_SELECT_MASK);
    switch(field){
      case NTP_PEER_SELECT_REJECT     :
#ifdef DEBUG
        fprintf(stderr, " rejected");
#endif
        break;
      case NTP_PEER_SELECT_SANE : 
#ifdef DEBUG
        fprintf(stderr, " sane");
#endif
        break;
      case NTP_PEER_SELECT_CORRECT : 
#ifdef DEBUG
        fprintf(stderr, " correct");
#endif
        break;
      case NTP_PEER_SELECT_CANDIDATE : 
#ifdef DEBUG
        fprintf(stderr, " candidate");
#endif
        break;
      case NTP_PEER_SELECT_OUTLIER : 
#ifdef DEBUG
        fprintf(stderr, " outlier");
#endif
        break;
      case NTP_PEER_SELECT_FAR : 
#ifdef DEBUG
        fprintf(stderr, " far");
#endif
        break;
      case NTP_PEER_SELECT_CLOSE : 
#ifdef DEBUG
        fprintf(stderr, " close");
#endif
        break;
    }
#ifdef DEBUG
    fprintf(stderr, "\n");
#endif

    field = GET_BITS(word, NTP_PEER_EVTCNT_SHIFT, NTP_PEER_EVTCNT_MASK);
#ifdef DEBUG
    fprintf(stderr, "%d events, ", field);
#endif

    field = GET_BITS(word, NTP_PEER_EVTCDE_SHIFT, NTP_PEER_EVTCDE_MASK);
#ifdef DEBUG
    fprintf(stderr, "last %d\n", field);
#endif
  }

  return 1;
}
#endif

