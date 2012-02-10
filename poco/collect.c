#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include "options.h"

#define PACKET_LEN 9728

struct collect_state{
  int c_verbose;
  int c_nfd;
  int c_ffd;
  FILE *c_meta;
  FILE *c_text;

  unsigned int c_have;

  struct timeval c_start;
  struct timeval c_stop;

  /*EXTRAS*/
  unsigned int c_valbits;
  unsigned int c_itemvals;
  unsigned int c_byteorder;
  unsigned int c_datatype;

  unsigned int c_size;
  unsigned int c_new;
  unsigned int c_extra;
  unsigned int c_direction;
  unsigned int c_mask;

  unsigned long c_rxp;
  unsigned long c_rxopts;
  unsigned long c_rxdata;
  unsigned long c_rxtotal;

  unsigned long c_errors;

  unsigned char c_buffer[PACKET_LEN];
};

int collect_file(char *name)
{
  int flags;

  flags = O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR;
#ifdef O_LARGEFILE
  flags |= O_LARGEFILE;
#endif

  return open(name, flags, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
}

int collect_socket(int port)
{
  int fd, size, result;
  struct sockaddr_in *addr, data;
  unsigned int len;

  addr = &data;

  fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (fd < 0){
    fprintf(stderr, "collect: unable to create socket: %s\n", strerror(errno));
    return -1;
  }

#define RESERVE_SOCKET (1024 * 1500)

  len = sizeof(int);
  result = getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, &len);
  if((result < 0) || (size < RESERVE_SOCKET)){
    size = RESERVE_SOCKET;
    result = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
    if(result < 0){
#ifdef SO_RCVBUFFORCE
      result = setsockopt(fd, SOL_SOCKET, SO_RCVBUFFORCE, &size, sizeof(size));
      if(result < 0){
#endif
        fprintf(stderr, "collect: unable to set receive buffer size to %d: %s\n", size, strerror(errno));
#ifdef SO_RCVBUFFORCE
      }
#endif
    }
  }

  len = sizeof(struct sockaddr_in);

  addr->sin_family = AF_INET;
  addr->sin_addr.s_addr = htonl(INADDR_ANY);
  addr->sin_port = htons(port);

  if(bind(fd, (struct sockaddr *) addr, len) < 0){
    fprintf(stderr, "collect: bind %s:%d failed: %s\n", inet_ntoa(addr->sin_addr), ntohs(addr->sin_port), strerror(errno));
    close(fd);
    return -1;
  }

  return fd;
}

void destroy_collect(struct collect_state *cs)
{
  if(cs == NULL){
    return;
  }

  if(cs->c_nfd){
    close(cs->c_nfd);
    cs->c_nfd = (-1);
  }
  if(cs->c_ffd){
    close(cs->c_ffd);
    cs->c_ffd = (-1);
  }

  free(cs);
}

struct collect_state *create_collect(char *binary, char *text, int port, int verbose)
{
  struct collect_state *cs;

  cs = malloc(sizeof(struct collect_state));
  if(cs == NULL){
    return NULL;
  }

  cs->c_meta = stderr;
  cs->c_text = NULL;

  cs->c_nfd = (-1);
  cs->c_ffd = (-1);

  cs->c_rxp = 0;
  cs->c_rxopts = 0;
  cs->c_rxdata = 0;
  cs->c_rxtotal = 0;
  cs->c_errors = 0;

  cs->c_new = 1;
  cs->c_size = 4;
  cs->c_extra = 0;
  cs->c_direction = 1;
  cs->c_mask = 0xFFFFFFFF;


  /* at this point, cs structure valid, can safely be destroyed */

  cs->c_verbose = verbose;
  if(text){
    if(strcmp(text, "-")){
      cs->c_text = fopen(text, "w");
      if(cs->c_text == NULL){
        fprintf(stderr, "create: unable to open %s for writing\n", text);
        destroy_collect(cs);
        return NULL;
      } else {
#ifdef DEBUG
        fprintf(stderr, "create: opened %s\n", text);
#endif
      }
    } else {
      cs->c_text = stdout;
    }
  } else {
    cs->c_text = NULL;
  }

  cs->c_nfd = collect_socket(port);
  if(cs->c_nfd < 0){
    destroy_collect(cs);
    return NULL;
  }

  cs->c_ffd = collect_file(binary);
  if(cs->c_ffd < 0){
    destroy_collect(cs);
    return NULL;
  }

  return cs;
}

int option_process(struct collect_state *cs, struct option_poco *op)
{
  uint16_t label, msw;
  uint32_t lsw;
  uint64_t big;
  int result;
  time_t t, now;

  label = ntohs(op->o_label);
  msw = ntohs(op->o_msw);
  lsw = ntohl(op->o_lsw);

  big = (msw * 0x1000000ULL) + lsw;

  result = 0;

  if(cs->c_meta){
    fprintf(cs->c_meta, "option 0x%04x: 0x%04x%08x ", label, msw, lsw);
  }

  switch(label){
    case INSTRUMENT_TYPE_OPTION_POCO :
      if(cs->c_meta){

        cs->c_valbits  = GET_VALBITS_INSTRUMENT_TYPE_POCO(lsw);
        cs->c_itemvals = GET_VALITMS_INSTRUMENT_TYPE_POCO(lsw);

        if(lsw & BIGENDIAN_INSTRUMENT_TYPE_POCO){
          cs->c_byteorder = 1;
        }
        else{
          cs->c_byteorder = 0;
        }

        if(lsw & UINT_TYPE_INSTRUMENT_TYPE_POCO){
          cs->c_datatype = 0;
        }
        else if(lsw & SINT_TYPE_INSTRUMENT_TYPE_POCO){
          cs->c_datatype = 1;
        }
        else{
          cs->c_datatype = 2;
        }

        fprintf(cs->c_meta, "instrument %d", msw);
        fprintf(cs->c_meta, " has %d", cs->c_itemvals);
        fprintf(cs->c_meta, " %s %s-endian values", 
            (lsw & COMPLEX_INSTRUMENT_TYPE_POCO) ? "complex" : "real", 
             cs->c_byteorder ? "big" : "little");
        fprintf(cs->c_meta, " each %d bits", cs->c_valbits);
      }
      break;
    case TIMESTAMP_OPTION_POCO :
      if(cs->c_meta) fprintf(cs->c_meta, "timestamp %llu", (unsigned long long)big);
      break;
    case PAYLOAD_LENGTH_OPTION_POCO :
      if(cs->c_meta) fprintf(cs->c_meta, "payload length %llu", (unsigned long long)big);
      break;
    case PAYLOAD_OFFSET_OPTION_POCO :
      if(cs->c_meta) fprintf(cs->c_meta, "payload offset %llu", (unsigned long long)big);
      break;
    case ADC_SAMPLE_RATE_OPTION_POCO :
      if(cs->c_meta) fprintf(cs->c_meta, "adc rate %lluHz", (unsigned long long)big);
      break;
    case FREQUENCY_CHANNELS_OPTION_POCO :
      if(cs->c_meta) fprintf(cs->c_meta, "%u frequency channels", lsw);
      break;
    case ANTENNAS_OPTION_POCO :
      if(cs->c_meta) fprintf(cs->c_meta, "%u antennas", lsw);
      break;
    case BASELINES_OPTION_POCO :
      if(cs->c_meta) fprintf(cs->c_meta, "%u baselines", lsw);
      break;
    case STREAM_CONTROL_OPTION_POCO :
      if(cs->c_meta) fprintf(cs->c_meta, "stream control: ");
      switch(lsw){
        case START_STREAM_CONTROL_POCO  :
          if(cs->c_meta) fprintf(cs->c_meta, "start");
          break;
        case SYNC_STREAM_CONTROL_POCO   :
          if(cs->c_meta) fprintf(cs->c_meta, "sync");
          break;
        case CHANGE_STREAM_CONTROL_POCO :
          if(cs->c_meta) fprintf(cs->c_meta, "change");
          break;
        case STOP_STREAM_CONTROL_POCO   :
          result = 1;
          if(cs->c_meta) fprintf(cs->c_meta, "stop");
          break;
        default :
          if(cs->c_meta) fprintf(cs->c_meta, "UNKNOWN");
          break;
      }
      break;
    case META_COUNTER_OPTION_POCO :
      if(cs->c_meta) fprintf(cs->c_meta, "meta packet %u", lsw);
      break;
    case SYNC_TIME_OPTION_POCO :
      t = big;
      time(&now);
      if(cs->c_meta){
        fprintf(cs->c_meta, "last sync at %lu (%lu seconds ago)", t, (now - t));
      }
      break;
    case BANDWIDTH_HZ_OPTION_POCO :
      if(cs->c_meta){
        fprintf(cs->c_meta, "%lluHz bandwidth", (unsigned long long)big);
      }
      break;
    case ACCUMULATIONS_OPTION_POCO :
      if(cs->c_meta) fprintf(cs->c_meta, "%llu accumulations", (unsigned long long)big);
      break;
    case TIMESTAMP_SCALE_OPTION_POCO :
      if(cs->c_meta) fprintf(cs->c_meta, "%llu timestamp scale", (unsigned long long)big);
      break;

  }

  if(cs->c_meta){
    fprintf(cs->c_meta, "\n");
  }

  return result;
}

int collect_precalculate(struct collect_state *cs)
{
  int k;
  /*Determine size*/
  switch(cs->c_valbits){
    case 8:
      cs->c_size = 1;
      break;
    case 16:
      cs->c_size = 2;
      break;
    case 32:
      cs->c_size= 4;
      break;
    default :
      /* TODO */
      break;
  }
  /*Check for Endianness*/
  if(cs->c_byteorder){
    cs->c_extra = 0;
    cs->c_direction = 1;
  }
  else{
    cs->c_extra = cs->c_size - 1;
    cs->c_direction = -1;
  }

  for(k = 0; k < cs->c_size; k++){
    cs->c_mask = cs->c_mask << 8;
  }

  return 0;

}

int collect_process(struct collect_state *cs)
{
  struct header_poco *hp;
  struct option_poco *op;
  unsigned int options, base, payload, i;
  int wr, or, result;
  int j, k;
  unsigned int value, compare, flag;
  unsigned int cnt = 1;

  result = 0;
  compare = 0;

  if(cs->c_have < 8){
    fprintf(stderr, "process: short packet (len=%u)\n", cs->c_have);
    return -1;
  }

  hp = (struct header_poco *) cs->c_buffer;

  if(hp->h_magic != htons(MAGIC_HEADER_POCO)){
    fprintf(stderr, "process: bad header magic 0x%04x\n", hp->h_magic);
    cs->c_errors++;
    return -1;
  }


  if(hp->h_version != htons(VERSION_HEADER_POCO)){
    fprintf(stderr, "process: odd version %d\n", ntohs(hp->h_version));
    cs->c_errors++;
    return -1;
  }

  options = ntohs(hp->h_options);
  base = (options + 1) * 8;

  if(base > cs->c_have){
    fprintf(stderr, "process: options larger than packet itself\n");
    cs->c_errors++;
    return -1;
  }

  payload = cs->c_have - base;

  if(cs->c_meta){
    fprintf(cs->c_meta, "frame %lu: options %u, length %u, payload %u\n", cs->c_rxp, options, cs->c_have, payload);
  }

  for(i = 0; i < options; i++){
    op = (struct option_poco *)(cs->c_buffer + ((i + 1) * 8));
    or = option_process(cs, op);
    if(or){
      result = or;
    }
  }

  cs->c_rxopts += options;

  /* at this point we have the number of data bytes in payload, and the start of data at at cp->c_bufer + base */

  if(payload){
    wr = write(cs->c_ffd, cs->c_buffer + base, payload);
    if(wr < payload){
      fprintf(stderr, "process: unable to write payload of %d\n", payload);
      return -1;
    }

    cs->c_rxdata += payload;

    if(cs->c_text != NULL){
      /* TODO: maybe write data out in human readable form to cs->c_text ? */
      /* fprintf(cs->c_text, "..."); */

      if(cs->c_new){
        collect_precalculate(cs);
        cs->c_new = 0;
      }

#ifdef DEBUG
      fprintf(stderr, "base=%d, payload=%d, size=%d\n", base, payload, cs->c_size);
#endif

      for(j = base; j < (base + payload); j += cs->c_size){
        value = 0;
        compare = cs->c_buffer[base + cs->c_extra];
        if(compare & 0x80){
          flag = 1;
        }
        for(k = 0; k < cs->c_size; k++){
          value *= 256;
          value += cs->c_buffer[j + cs->c_extra + (cs->c_direction * k)];
        }
        /*Check for data type:unsigned,signed or float*/
        if(!cs->c_datatype){
          fprintf(cs->c_text,"%u%c", value, (cnt % 24 != 0) ? ' ' : '\n');
        }
        else if(cs->c_datatype == 1){
          if(flag){
            value = (value | cs->c_mask);
          }
          fprintf(cs->c_text,"%d%c", value, (cnt % 24 != 0) ? ' ' : '\n');
        }
        else{
          /*TODO:Floating point case*/
        }
        flag = 0;
        cnt++;

        if(cnt == 24){
          cnt = 0;
        }
      }
    }
  }

  return result;
}

int collect_loop(struct collect_state *cs)
{
  int rr, result, i;

  for(;;){
    rr = recv(cs->c_nfd, cs->c_buffer, PACKET_LEN, 0);
#ifdef DEBUG
    fprintf(stderr, "loop: received with code %d\n", rr);
#endif
    if(rr < 0){
      switch(errno){
        case EAGAIN :
        case EINTR :
          continue; /* WARNING */
        default :
          fprintf(stderr, "collect: receive failed %s\n", strerror(errno));
          gettimeofday(&(cs->c_stop), NULL);
          return -1;
      }
    }

    if(cs->c_rxp == 0){
      gettimeofday(&(cs->c_start), NULL);
    }

    cs->c_have = rr;

    cs->c_rxp++;
    cs->c_rxtotal += rr;

    if(cs->c_verbose > 1){
      fprintf(stderr, "read: got packet of %d\n", rr);
      if(cs->c_verbose > 2){
        fprintf(stderr, "read: data");
        for(i = 0; i < cs->c_have; i++){
          fprintf(stderr, " %02x", cs->c_buffer[i]);
        }
        fprintf(stderr, "\n");
      }
    }

    result = collect_process(cs);

    if(result){
      gettimeofday(&(cs->c_stop), NULL);
      return (result > 0) ? 0 : -1;
    }
  }
}

int collect_stats(struct collect_state *cs)
{
  double from, to, delta, rate;

  if(cs->c_meta){
    fprintf(cs->c_meta, "frames received:     %lu\n", cs->c_rxp);
    fprintf(cs->c_meta, "malformed frames:    %lu\n", cs->c_errors);
    fprintf(cs->c_meta, "total option count:  %lu\n", cs->c_rxopts);
    fprintf(cs->c_meta, "total data bytes:    %lu\n", cs->c_rxdata);
    fprintf(cs->c_meta, "overall bytes seen:  %lu\n", cs->c_rxtotal);

    fprintf(cs->c_meta, "start:    %lu.%06lus\n", cs->c_start.tv_sec, cs->c_start.tv_usec);
    fprintf(cs->c_meta, "stop:     %lu.%06lus\n", cs->c_stop.tv_sec, cs->c_stop.tv_usec);

    fflush(cs->c_meta);

    from = cs->c_start.tv_sec * 1000000 + cs->c_start.tv_usec;
    to   = cs->c_stop.tv_sec * 1000000 + cs->c_stop.tv_usec;

    delta = (to - from) / 1000.0;
    rate = (cs->c_rxtotal) / delta;
    fprintf(cs->c_meta, "rate: %fkb/s\n", rate);
  }

  return 0;
}


int main(int argc, char **argv)
{
  int i, j, c;
  struct collect_state *cs;
  int verbose;

  int port = 0;
  char *binary = NULL;
  char *text = NULL;
  verbose = 1;

  i = 1;
  j = 1;
  while (i < argc) {
    if (argv[i][0] == '-') {
      c = argv[i][j];
      switch (c) {
        case '\0':
          j = 1;
          i++;
          break;
        case '-':
          j++;
          break;
        case 'q':
          verbose = 0;
          i++;
          break;
        case 'v':
          verbose++;
          i++;
          break;
        case 'h' :
          fprintf(stderr, "usage: %s [-o binary-output] [-d decoded-output] [-p receive-port]\n", argv[0]);
          return 0;
          break;
        case 'd' :
          j++;
          if (argv[i][j] == '\0') {
            j = 0;
            i++;
          }
          if(i >= argc){
            text == NULL;
          } else {
            text = argv[i] + j;
          }
          i++;
          j = 1;
          break;
        case 'o' :
        case 'p' :
          j++;
          if (argv[i][j] == '\0') {
            j = 0;
            i++;
          }
          if (i >= argc) {
            fprintf(stderr, "%s: option -%c requires a parameter\n", argv[0], c);
            return EX_USAGE;
          }
          switch(c){
            case 'o' :
              binary = argv[i] + j;
              break;
            case 'p' :
              port = atoi(argv[i] + j);
              break;
          }
          i++;
          j = 1;
          break;
        default:
          fprintf(stderr, "%s: unknown option -%c\n", argv[0], c);
          return 1;
          break;
      }
    } else {
      fprintf(stderr, "%s: extra argument %s\n", argv[0], argv[i]);
      return EX_USAGE;
    }
  }

  if((port <= 0) || (port > 0xffff)){
    fprintf(stderr, "%s: invalid port %d\n", argv[0], port);
    return EX_USAGE;
  }

  if(binary == NULL){
    fprintf(stderr, "%s: need an output filename\n", argv[0]);
    return EX_USAGE;
  }

  cs = create_collect(binary, text, port, verbose);
  if(cs == NULL){
    fprintf(stderr, "%s: unable to set up\n", argv[0]);
    return EX_USAGE;
  }

  collect_loop(cs);

  collect_stats(cs);

  destroy_collect(cs);

  return 0;
}
