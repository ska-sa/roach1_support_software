#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <netdb.h>

#include <sys/time.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <katcp.h>
#include <katpriv.h> /* for timeval arith */

#include "core.h"
#include "modes.h"
#include "poco.h"
#include "misc.h"
#include "registers.h"

#define DRAM_MAGIC 0x6472616d

#define DRAM_WORD_SIZE  (4 * 2 * 2)
#define DRAM_PAD_FACTOR  2

int toggle_dram_poco(struct katcp_dispatch *d, void *data, int start);
void destroy_dram_poco(struct katcp_dispatch *d, void *data);

#ifdef DEBUG
void sane_dram(struct dram_poco *r)
{
  if(r == NULL){
    fprintf(stderr, "dram: r is null\n");
    abort();
  }
  if(r->r_magic != DRAM_MAGIC){
    fprintf(stderr, "dram: bad magic 0x%08x, expected 0x%08x\n", r->r_magic, DRAM_MAGIC);
    abort();
  }
}
#else
#define sane_dram(r)
#endif

/* create is responsible for adding its functions to the cp structure */

int create_dram_poco(struct katcp_dispatch *d, struct capture_poco *cp, unsigned int count)
{
  struct dram_poco *r;

  if(cp->c_dump != NULL){
#ifdef DEBUG
    fprintf(stderr, "dram: capture %s already associated with a data generator\n", cp->c_name);
    abort();
#endif
    return -1;
  }

  r = malloc(sizeof(struct dram_poco));
  if(r == NULL){
#ifdef DEBUG
    fprintf(stderr, "dram: unable to allocate %d bytes for state\n", sizeof(struct dram_poco));
#endif
    return -1;
  }

  r->r_magic = DRAM_MAGIC;
  r->r_capture = NULL;

  r->r_ctl = NULL;
  r->r_sts = NULL;
  r->r_dram = NULL;
  r->r_stamp = NULL;

  r->r_count = count; /* number of elements dumped (512 * 12) */
  r->r_chunk = r->r_count * (DRAM_WORD_SIZE / DRAM_PAD_FACTOR) ; /* the maximum number of bytes we would ever want to send */
  r->r_state = 0;
  r->r_number = 0; /* increase if we ever have more than one */

  r->r_lost = 0;
  r->r_got = 0;
  r->r_nul = 0;
  r->r_txp = 0;

  r->r_buffer = NULL;
  r->r_size = 0;

  /***/

  r->r_size = r->r_count * DRAM_WORD_SIZE;
  r->r_buffer = malloc(r->r_size);
  if(r->r_buffer == NULL){
    destroy_dram_poco(d, cp);
    return -1;
  }

  /* trim the maximum to the number we can send - limited by packet size */
  while(r->r_chunk > (cp->c_size - DATA_OVERHEAD_POCO)){
    r->r_chunk = r->r_chunk / 2;
  }
  /* TODO - imperfect chunk size, improve */
#ifdef DEBUG
  fprintf(stderr, "dram: selecting a data chunk size of %ubytes, dram is %u words\n", r->r_chunk, r->r_count);
#endif

  cp->c_dump = r;
  cp->c_toggle = &toggle_dram_poco;
  cp->c_destroy = &destroy_dram_poco;

  r->r_capture = cp;

  return 0;
}

int run_dram_poco(struct katcp_dispatch *d, void *data)
{
  struct dram_poco *r;
  struct capture_poco *cp;
  struct state_poco *sp;

  uint32_t status, control, stamp, value;
  int rr, wr, i, j, sw, valid;
  void *payload;
  unsigned int have;

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "running dram dump function");

  r = data;

  sane_dram(r);

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "left poco mode hence disabling dram %d", r->r_number);
    return -1;
  }

  cp = r->r_capture;

  rr = read_pce(d, r->r_sts, &value, 0, 4);
  if(rr != 4){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to read address of dram %d", r->r_number);
    return -1;
  }
  status = value & (IS_DONE_VACC_STATUS0 | IS_BUFFER_OVERFLOW_VACC_STATUS0 | IS_OVERWRITE_VACC_STATUS0 | IS_ACCUMULATOR_OVERFLOW_VACC_STATUS0 | IS_READY_PHY_VACC_STATUS0);

  if(status == IS_READY_PHY_VACC_STATUS0){
    log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "nothing ready yet");
    r->r_nul++;
    return 0;
  }

  /* something bad has happend */
  if(!(status & (IS_READY_PHY_VACC_STATUS0 | IS_BUFFER_OVERFLOW_VACC_STATUS0))){
    log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "status is 0x%02x", status);
    /* dram has gone away */
    if(status & IS_READY_PHY_VACC_STATUS0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "lost dram phy ready");
    }
    /* gateware had to drop things to the floor */
    if(status & IS_BUFFER_OVERFLOW_VACC_STATUS0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "dsp pipeline overflow");
    }
    return -1;
  }

  rr = read_pce(d, r->r_stamp, &stamp, 0, 4);
  if(rr != 4){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to read timestamp for dram %d", r->r_number);
    return -1;
  }

  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "vector count is %u", stamp);
  if(stamp < cp->c_ts_lsw){
    cp->c_ts_msw++;
  }
  cp->c_ts_lsw = stamp;

  valid = 0;

  /* data got clobbered, we were too slow or read too often */
  if(status & IS_OVERWRITE_VACC_STATUS0){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "lost integrations before count %u", stamp);
    r->r_lost++;
  }

  if(status & IS_ACCUMULATOR_OVERFLOW_VACC_STATUS0){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "accumulator overflow before count %u", stamp);
    r->r_lost++;
  }

  if(status & IS_DONE_VACC_STATUS0){ /* read out if data available */
    r->r_got++;

#ifdef DEBUG
    memset(r->r_buffer, 0xc5, r->r_size);
#endif

    rr = read_pce(d, r->r_dram, r->r_buffer, 0, r->r_size);
    if(rr < r->r_size){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to acquire full dram buffer of %u bytes", r->r_size);
      return -1;
    }

#if DEBUG > 1
    j = 0;
    fprintf(stderr, "dump:");
    for(i = 0; i < r->r_size; i++){
      if(r->r_buffer[i] != 0){
        if(j < 32){
          fprintf(stderr, " [%d]=0x%x", i, r->r_buffer[i]);
        }
        j++;
      }
    }
    fprintf(stderr, "(%d nonzero bytes)\n", j);
#endif

    valid = 1;

  } else {
    log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "dram not yet ready and errors 0x%02x", status);
    r->r_nul++;
    return -1;
  }

  /* clear bits soon after reading data */
  control = sp->p_source_state;
  wr = write_pce(d, r->r_ctl, &control, 0, 4);
  if(wr != 4){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to zero dram control");
    return -1;
  }
  /* WARNING: assumes that state bits match control */
  control = sp->p_source_state | (status & (IS_DONE_VACC_STATUS0 | IS_BUFFER_OVERFLOW_VACC_STATUS0 | IS_OVERWRITE_VACC_STATUS0 | IS_ACCUMULATOR_OVERFLOW_VACC_STATUS0));
  wr = write_pce(d, r->r_ctl, &control, 0, 4);
  if(wr != 4){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to reset dram control");
    return -1;
  }

  rr = read_pce(d, r->r_sts, &value, 0, 4);
  if(rr != 4){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to read address of dram %d", r->r_number);
    return -1;
  }
  status = value & (IS_DONE_VACC_STATUS0 | IS_BUFFER_OVERFLOW_VACC_STATUS0 | IS_OVERWRITE_VACC_STATUS0 | IS_ACCUMULATOR_OVERFLOW_VACC_STATUS0 | IS_READY_PHY_VACC_STATUS0);

  if((status & (IS_DONE_VACC_STATUS0 | IS_BUFFER_OVERFLOW_VACC_STATUS0 | IS_OVERWRITE_VACC_STATUS0 | IS_ACCUMULATOR_OVERFLOW_VACC_STATUS0 | IS_READY_PHY_VACC_STATUS0)) != IS_READY_PHY_VACC_STATUS0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "problematic status register content 0x%x after read", status);
    return -1;
  }

#if 0
    if((sw % DRAM_WORD_SIZE) || (have % DRAM_WORD_SIZE)){
      /* error, but this really should not happen */
    }
#endif

  if(valid){ /* send data across network */
    have = 0;
    do {
      sw = r->r_size - have;
      if(sw > (r->r_chunk * DRAM_PAD_FACTOR)){
        sw = r->r_chunk * DRAM_PAD_FACTOR;
      }
      payload = data_udp_poco(d, cp, have / DRAM_PAD_FACTOR, sw / DRAM_PAD_FACTOR);
      if(payload == NULL){
        log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to acquire dram payload space of %d", sw);
        return -1;
      }

#ifdef DEBUG
      memset(payload, 0x5c, sw / DRAM_PAD_FACTOR);
#endif

      i = 0;
      j = 0;

      do {
        memcpy(payload + j, r->r_buffer + have + i, DRAM_WORD_SIZE / DRAM_PAD_FACTOR);
        j += DRAM_WORD_SIZE / DRAM_PAD_FACTOR;
        i += DRAM_WORD_SIZE;
      } while(i < sw);

      log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "frame[%d]: %02x %02x %02x %02x %02x %02x %02x %02x", r->r_txp, r->r_buffer[0], r->r_buffer[1], r->r_buffer[2], r->r_buffer[3], r->r_buffer[4], r->r_buffer[5], r->r_buffer[6], r->r_buffer[7]);

      if(tx_udp_poco(d, cp) >= 0){
        r->r_txp++;
      }

      have += sw;

    } while(have < r->r_size);
  }

  return 0;
}

int toggle_dram_poco(struct katcp_dispatch *d, void *data, int start)
{
  struct dram_poco *r;
  struct timeval period;
  struct state_poco *sp;
  uint32_t value, control, status;
  int rr, wr;

  r = data;

  sane_dram(r);

  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "toggling dram capture %s", start ? "on" : "off");

  if(start == 0){ /* stop */

    log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "read %u valid dram blocks", r->r_got);
    r->r_got = 0;

    log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "checked %u not ready dram blocks", r->r_nul);
    r->r_nul = 0;

    log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "encountered %u overruns", r->r_lost);
    r->r_lost = 0;

    log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "sent %u dram data frames", r->r_txp);
    r->r_txp = 0;

    /* TODO - have an up flag to check if not shutting something which isn't running */

    discharge_timer_katcp(d, r);
    return 0;
  }

  sp = get_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return -1;
  }

  /* else start */
#if 0
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "provisional poll rate is %lu.%06lus", tv->tv_sec, tv->tv_usec);
#endif

  r->r_sts = by_name_pce(d, STATUS0_POCO_REGISTER);
  r->r_ctl = by_name_pce(d, CONTROL0_POCO_REGISTER);
  r->r_dram = by_name_pce(d, DRAM_MEMORY_POCO_REGISTER);
  r->r_stamp = by_name_pce(d, ACC_TIMESTAMP_POCO_REGISTER);
  if(!(r->r_ctl && r->r_sts && r->r_dram && r->r_stamp)){
#ifdef DEBUG
    fprintf(stderr, "dram: unable to locate registers\n");
#endif
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "dram unable to locate registers");
    return -1;
  }

  /* set accumulation length */
  if(write_name_pce(d, ACC_LEN_POCO_REGISTER, &(sp->p_dump_count), 0, 4) != 4){
    return -1;
  }

  /* clear overflow bits, we are starting afresh */
  rr = read_pce(d, r->r_sts, &status, 0, 4);
  if(rr != 4){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to read address of dram %d", r->r_number);
    return -1;
  }
  control = sp->p_source_state;
  wr = write_pce(d, r->r_ctl, &control, 0, 4);
  if(wr != 4){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to zero dram control");
    return -1;
  }
  control = sp->p_source_state | (status & (IS_DONE_VACC_STATUS0 | IS_BUFFER_OVERFLOW_VACC_STATUS0 | IS_OVERWRITE_VACC_STATUS0 | IS_ACCUMULATOR_OVERFLOW_VACC_STATUS0));
  wr = write_pce(d, r->r_ctl, &control, 0, 4);
  if(wr != 4){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to reset dram control");
    return -1;
  }

  value = sp->p_accumulation_length / 2;
  period.tv_sec = value / MSECPERSEC_POCO;
  period.tv_usec = (value % MSECPERSEC_POCO) * MSECPERSEC_POCO;
  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "setting poll interval to %lu.%06lus for accumulation length of %ums", period.tv_sec, period.tv_usec, sp->p_accumulation_length);

  if(register_every_tv_katcp(d, &period, &run_dram_poco, r) < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to register dram collection timer");
    return -1;
  }

  return 0;
}

void destroy_dram_poco(struct katcp_dispatch *d, void *data)
{
  struct dram_poco *r;

  if(data == NULL){
    return;
  }

  r = data;

  sane_dram(r);

  r->r_size = 0;
  if(r->r_buffer){
    free(r->r_buffer);
    r->r_buffer = NULL;
  }

  if(r->r_capture){
#ifdef DEBUG
    if(r->r_capture->c_dump != NULL){
      if(r->r_capture->c_dump != r){
        fprintf(stderr, "dram: major logic failure: crosslinked dram and capture objects\n");
        abort();
      }
    }
#endif
    r->r_capture->c_dump = NULL;
    r->r_capture->c_toggle = NULL;
    r->r_capture = NULL;
  }

  discharge_timer_katcp(d, r);
}
