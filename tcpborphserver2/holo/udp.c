#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <arpa/inet.h>

#include "core.h"
#include "holo.h"
#include "holo-options.h"
#include "holo-config.h"
#include "modes.h"

static void reset_udp_holo(struct capture_holo *ch)
{
#ifdef DEBUG
  if(sizeof(struct option_poco) != 8){
    fprintf(stderr, "udp: major logic problems: option is %lu bytes\n", sizeof(struct option_poco));
    abort();
  }
#endif
  ch->c_used = 8;
  ch->c_sealed = 0;
  ch->c_failures = 0;
  ch->c_options = 0;
}

int option_udp_holo(struct katcp_dispatch *d, struct capture_holo *ch, uint16_t label, uint16_t msw, uint32_t lsw)
{
  struct option_poco *op;

#ifdef DEBUG
  if(ch->c_sealed > 0){
    fprintf(stderr, "option: refusing to add option to sealed data packed\n");
    abort();
  }
  if(ch->c_used % 4){
    fprintf(stderr, "option: used %u not word aligned\n", ch->c_used);
    abort();
  }
#endif

  if(ch->c_used + 8 > ch->c_size){
    ch->c_failures++;
    return -1;
  }

  op = (struct option_poco *)(ch->c_buffer + ch->c_used);

  op->o_label = htons(label);
  op->o_msw = htons(msw);
  op->o_lsw = htonl(lsw);

  ch->c_used += sizeof(struct option_poco);
  ch->c_options++;

  return 0;
}

void *data_udp_holo(struct katcp_dispatch *d, struct capture_holo *ch, unsigned int offset, unsigned int len)
{
  if((ch->c_used + 8 + len) > ch->c_size){
#ifdef DEBUG
    fprintf(stderr, "udp: unable to meet request of %d (used %u/%u)\n", len, ch->c_used, ch->c_size);
#endif
    return NULL;
  }

  option_udp_holo(d, ch, PAYLOAD_LENGTH_OPTION_HOLO, 0, len);
  option_udp_holo(d, ch, PAYLOAD_OFFSET_OPTION_HOLO, 0, offset);
  option_udp_holo(d, ch, TIMESTAMP_OPTION_HOLO, ch->c_ts_msw, ch->c_ts_lsw);

  //log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "data: major logic problem: ch->c_size=%u, data overhead is %u, offset:%u, length =%u\n", ch->c_size, ch->c_used, offset, len);

#ifdef DEBUG
  if(ch->c_used != DATA_OVERHEAD_HOLO){ /* check number of options added */
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "data: major logic problem: data overhead is %u, offset:%u, length =%u\n", ch->c_used, offset, len);
    fprintf(stderr, "data: major logic problem: data overhead is %u, offset:%u, length =%u\n", ch->c_used, offset, len);
    abort();
  }
#endif

  ch->c_sealed = ch->c_used;
  ch->c_used += len;

  return (ch->c_buffer + ch->c_sealed);
}

int meta_udp_holo(struct katcp_dispatch *d, struct capture_holo *ch, int control)
{
  struct state_holo *sh;
  uint32_t format;

#ifdef DEBUG
  fprintf(stderr, "meta: issuing meta packet code %d\n", control);
#endif

  sh = get_mode_katcp(d, POCO_HOLO_MODE);
  if(sh == NULL){
    return -1;
  }

  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "issuing meta packet code %d", control);

  format = COMPLEX_INSTRUMENT_TYPE_HOLO | BIGENDIAN_INSTRUMENT_TYPE_HOLO | SINT_TYPE_INSTRUMENT_TYPE_HOLO | SET_VALBITS_INSTRUMENT_TYPE_HOLO(32) | SET_VALITMS_INSTRUMENT_TYPE_HOLO(4);
  option_udp_holo(d, ch, INSTRUMENT_TYPE_OPTION_HOLO, POCO_INSTRUMENT_TYPE_HOLO, format);

  option_udp_holo(d, ch, INSTANCE_ID_OPTION_HOLO, 0, 0);

#if 0
  option_udp_holo(d, ch, TIMESTAMP_OPTION_HOLO, 0, 0);
  /* no PAYLOAD LENGTH in this packet */
  /* no PAYLOAD OFFSET in this packet */
#endif

  option_udp_holo(d, ch, ADC_SAMPLE_RATE_OPTION_HOLO, 0, sh->h_dsp_clock);

  option_udp_holo(d, ch, TIMESTAMP_OPTION_HOLO, 0, sh->h_time_stamp);
  option_udp_holo(d, ch, FREQUENCY_CHANNELS_OPTION_HOLO, 0, sh->h_fft_window);
  option_udp_holo(d, ch, ANTENNAS_OPTION_HOLO, 0, HOLO_ANT_COUNT);
  option_udp_holo(d, ch, BASELINES_OPTION_HOLO, 0, HOLO_BASELINE_VALUE);
  //option_udp_holo(d, ch, BASELINES_OPTION_HOLO, 0, (HOLO_ANT_COUNT * (HOLO_ANT_COUNT + 1)) / 2);

  option_udp_holo(d, ch, STREAM_CONTROL_OPTION_HOLO, STREAM_CONTROL_OPTION_HOLO, control); /* only one packet sofar, label it as last */
  option_udp_holo(d, ch, META_COUNTER_OPTION_HOLO, 0, 0);
   
  option_udp_holo(d, ch, SYNC_TIME_OPTION_HOLO, 0, sh->h_sync_time.tv_sec);

  option_udp_holo(d, ch, CENTER_FREQUENCY_HZ_OPTION_HOLO, 0, sh->h_center_freq);
  option_udp_holo(d, ch, BANDWIDTH_HZ_OPTION_HOLO, 0, HOLO_TOTAL_BANDWIDTH);
  option_udp_holo(d, ch, ACCUMULATIONS_OPTION_HOLO, 0, sh->h_dump_count);

  option_udp_holo(d, ch, TIMESTAMP_SCALE_OPTION_HOLO, 0, HOLO_TIMESTAMP_SCALE_FACTOR);

  if(ch->c_failures){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to issue meta packeet");
    return -1;
  }

  return 0;
}

int tx_udp_holo(struct katcp_dispatch *d, struct capture_holo *ch)
{
  struct header_poco *hp;
  int result;
  int wr;

#ifdef DEBUG
  if(((ch->c_options + 1) * 8) > ch->c_used){
    fprintf(stderr, "tx: %u options, %u bytes allotted\n", ch->c_options, ch->c_used);
    abort();
  }
  if(ch->c_used > ch->c_size){
    fprintf(stderr, "tx: %u used, %u size\n", ch->c_used, ch->c_size);
    abort();
  }
#endif

  hp = (struct header_poco *) ch->c_buffer;

  hp->h_magic    = htons(MAGIC_HEADER_HOLO);
  hp->h_version  = htons(VERSION_HEADER_HOLO);
  hp->h_reserved = htons(0);
  hp->h_options  = htons(ch->c_options);

  wr = write(ch->c_fd, ch->c_buffer, ch->c_used);
  
  if(wr < ch->c_used){
#ifdef DEBUG
    fprintf(stderr, "tx: write failed: %d instead of %u (%s)\n", wr, ch->c_used, (wr < 0) ? strerror(errno) : "short write");
#endif
    ch->c_failures++;
  }

  result = ch->c_failures ? (-1) : 0;

  reset_udp_holo(ch);

  return result;
}

int init_udp_holo(struct katcp_dispatch *d, struct capture_holo *ch)
{
  int fd;
  struct sockaddr_in sa;

  if(ch->c_fd >= 0){
    log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "udp: closing previous file descriptor");
    close(ch->c_fd);
    ch->c_fd = (-1);
  }

  if(ch->c_port == htons(0)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "udp: no port set");
    return -1;
  }

  if(ch->c_ip == htonl(0)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "udp: no ip address");
    return -1;
  }

  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = ch->c_ip;
  sa.sin_port = ch->c_port;

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

  ch->c_fd = fd;

  reset_udp_holo(ch);

  return 0;
}

