#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <arpa/inet.h>

#include "core.h"
#include "poco.h"
#include "options.h"
#include "modes.h"

static void reset_udp_poco(struct capture_poco *cp)
{
#ifdef DEBUG
  if(sizeof(struct option_poco) != 8){
    fprintf(stderr, "udp: major logic problems: option is %d bytes\n", sizeof(struct option_poco));
    abort();
  }
#endif
  cp->c_used = 8;
  cp->c_sealed = 0;
  cp->c_failures = 0;
  cp->c_options = 0;
}

int option_udp_poco(struct katcp_dispatch *d, struct capture_poco *cp, uint16_t label, uint16_t msw, uint32_t lsw)
{
  struct option_poco *op;

#ifdef DEBUG
  if(cp->c_sealed > 0){
    fprintf(stderr, "option: refusing to add option to sealed data packed\n");
    abort();
  }
  if(cp->c_used % 4){
    fprintf(stderr, "option: used %u not word aligned\n", cp->c_used);
    abort();
  }
#endif

  if(cp->c_used + 8 > cp->c_size){
    cp->c_failures++;
    return -1;
  }

  op = (struct option_poco *)(cp->c_buffer + cp->c_used);

  op->o_label = htons(label);
  op->o_msw = htons(msw);
  op->o_lsw = htonl(lsw);

  cp->c_used += sizeof(struct option_poco);
  cp->c_options++;

  return 0;
}

void *data_udp_poco(struct katcp_dispatch *d, struct capture_poco *cp, unsigned int offset, unsigned int len)
{
  if((cp->c_used + 8 + len) > cp->c_size){
#ifdef DEBUG
    fprintf(stderr, "udp: unable to meet request of %d (used %u/%u)\n", len, cp->c_used, cp->c_size);
#endif
    return NULL;
  }

  option_udp_poco(d, cp, PAYLOAD_LENGTH_OPTION_POCO, 0, len);
  option_udp_poco(d, cp, PAYLOAD_OFFSET_OPTION_POCO, 0, offset);
  option_udp_poco(d, cp, TIMESTAMP_OPTION_POCO, cp->c_ts_msw, cp->c_ts_lsw);

#ifdef DEBUG
  if(cp->c_used != DATA_OVERHEAD_POCO){ /* check number of options added */
    fprintf(stderr, "data: major logic problem: data overhead is %u\n", cp->c_used);
    abort();
  }
#endif

  cp->c_sealed = cp->c_used;
  cp->c_used += len;

  return (cp->c_buffer + cp->c_sealed);
}

int meta_udp_poco(struct katcp_dispatch *d, struct capture_poco *cp, int control)
{
  struct state_poco *sp;
  uint32_t format;

#ifdef DEBUG
  fprintf(stderr, "meta: issuing meta packet code %d\n", control);
#endif

  sp = get_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return -1;
  }

  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "issuing meta packet code %d", control);

  format = COMPLEX_INSTRUMENT_TYPE_POCO | BIGENDIAN_INSTRUMENT_TYPE_POCO | SINT_TYPE_INSTRUMENT_TYPE_POCO | SET_VALBITS_INSTRUMENT_TYPE_POCO(32) | SET_VALITMS_INSTRUMENT_TYPE_POCO(12);
  option_udp_poco(d, cp, INSTRUMENT_TYPE_OPTION_POCO, POCO_INSTRUMENT_TYPE_POCO, format);

  option_udp_poco(d, cp, INSTANCE_ID_OPTION_POCO, 0, 0);

#if 0
  option_udp_poco(d, cp, TIMESTAMP_OPTION_POCO, 0, 0);
  /* no PAYLOAD LENGTH in this packet */
  /* no PAYLOAD OFFSET in this packet */
#endif

  option_udp_poco(d, cp, ADC_SAMPLE_RATE_OPTION_POCO, 0, sp->p_dsp_clock * POCO_ADC_DSP_FACTOR);

  option_udp_poco(d, cp, FREQUENCY_CHANNELS_OPTION_POCO, 0, sp->p_fft_window);
  option_udp_poco(d, cp, ANTENNAS_OPTION_POCO, 0, POCO_ANT_COUNT);
  option_udp_poco(d, cp, BASELINES_OPTION_POCO, 0, (POCO_ANT_COUNT * (POCO_ANT_COUNT + 1)) / 2);

  option_udp_poco(d, cp, STREAM_CONTROL_OPTION_POCO, STREAM_CONTROL_OPTION_POCO, control); /* only one packet sofar, label it as last */
  option_udp_poco(d, cp, META_COUNTER_OPTION_POCO, 0, 0);
   
  option_udp_poco(d, cp, SYNC_TIME_OPTION_POCO, 0, sp->p_sync_time.tv_sec);

  option_udp_poco(d, cp, BANDWIDTH_HZ_OPTION_POCO, 0, sp->p_dsp_clock * 2);
  option_udp_poco(d, cp, ACCUMULATIONS_OPTION_POCO, 0, sp->p_dump_count * sp->p_pre_accumulation);

  /* MAYBE multiply by factor 4 to get ADC samples ? */
  option_udp_poco(d, cp, TIMESTAMP_SCALE_OPTION_POCO, 0, (sp->p_fft_window * sp->p_pre_accumulation) / 2);

  if(cp->c_failures){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to issue meta packeet");
    return -1;
  }

  return 0;
}

int tx_udp_poco(struct katcp_dispatch *d, struct capture_poco *cp)
{
  struct header_poco *hp;
  int result;
  int wr;

#ifdef DEBUG
  if(((cp->c_options + 1) * 8) > cp->c_used){
    fprintf(stderr, "tx: %u options, %u bytes allotted\n", cp->c_options, cp->c_used);
    abort();
  }
  if(cp->c_used > cp->c_size){
    fprintf(stderr, "tx: %u used, %u size\n", cp->c_used, cp->c_size);
    abort();
  }
#endif

  hp = (struct header_poco *) cp->c_buffer;

  hp->h_magic    = htons(MAGIC_HEADER_POCO);
  hp->h_version  = htons(VERSION_HEADER_POCO);
  hp->h_reserved = htons(0);
  hp->h_options  = htons(cp->c_options);

  wr = write(cp->c_fd, cp->c_buffer, cp->c_used);
  
  if(wr < cp->c_used){
#ifdef DEBUG
    fprintf(stderr, "tx: write failed: %d instead of %u (%s)\n", wr, cp->c_used, (wr < 0) ? strerror(errno) : "short write");
#endif
    cp->c_failures++;
  }

  result = cp->c_failures ? (-1) : 0;

  reset_udp_poco(cp);

  return result;
}

int init_udp_poco(struct katcp_dispatch *d, struct capture_poco *cp)
{
  int fd;
  struct sockaddr_in sa;

  if(cp->c_fd >= 0){
    log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "udp: closing previous file descriptor");
    close(cp->c_fd);
    cp->c_fd = (-1);
  }

  if(cp->c_port == htons(0)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "udp: no port set");
    return -1;
  }

  if(cp->c_ip == htonl(0)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "udp: no ip address");
    return -1;
  }

  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = cp->c_ip;
  sa.sin_port = cp->c_port;

  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if(fd < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "udp: unable to create socket: %s", strerror(errno));
    return -1;
  }

  if(connect(fd, (struct sockaddr *)&sa, sizeof(struct sockaddr_in)) < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "udp: unable to connect: %s", strerror(errno));
    close(fd);
    return -1;
  }

  cp->c_fd = fd;

  reset_udp_poco(cp);

  return 0;
}

