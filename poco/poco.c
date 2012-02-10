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
#include "config.h"
#include "registers.h"

/* needed for meta packet, unclear if we should dive into that here */
#include "options.h"

/************************************************************************/

#if 0 /* now done inside toggle_dram */
void time_accumulator_poco(struct katcp_dispatch *d, struct timeval *tv)
{
  struct state_poco *sp;
  unsigned int value;

  sp = get_mode_katcp(d, POCO_POCO_MODE);
  if(sp){
    value = sp->p_accumulation_length / 2;

#ifdef DEBUG
    fprintf(stderr, "time: adjusting poll interval (%u ms) to half of length\n", value);
#endif

    tv->tv_sec = value / 1000;
    tv->tv_usec = (value % 1000) * 1000;
    if(log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "setting poll interval for dump to %lu.%06lus", tv->tv_sec, tv->tv_usec) < 0){
      fprintf(stderr, "time: unable to log a message\n");
      abort();
    }

#ifdef DEBUG
  } else {
    fprintf(stderr, "time: unable to acquire poco mode state\n");
    abort();
#endif
  }
}

int prepare_accumulator_poco(struct katcp_dispatch *d)
{
#if 0
  struct poco_core_entry *acc_ctl, *timing_ctl;
#endif
  struct state_poco *sp;

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "preparing accumulator");

  sp = get_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return -1;
  }

  /* input source now set at startup */
#if 0
  /* select input source */
  value = 1;
  if(write_name_pce(d, VACC_SRC_POCO_REGISTER, &value, 0, 4) != 4){
    return -1;
  }
#endif

  /* set dump rate */
  if(write_name_pce(d, ACC_LEN_POCO_REGISTER, &(sp->p_dump_count), 0, 4) != 4){
    return -1;
  }

#if 0
  /* TODO hold timing registe low */
  timing_ctl = by_name_pce(d, TIMING_CTL_POCO_REGISTER);
  if(timing_ctl == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to locate register %s", TIMING_CTL_POCO_REGISTER);
    return -1;
  }
  value = 0;
  if(write_pce(d, timing_ctl, &value, 0, 4) != 4){
    return -1;
  }
  value = TIMING_ARM_FLAG;
  if(write_pce(d, timing_ctl, &value, 0, 4) != 4){
    return -1;
  }

  /* cache the handle for the acc_ctl register */
  acc_ctl = by_name_pce(d, ACC_CTL_POCO_REGISTER);
  if(acc_ctl == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to locate register %s", ACC_CTL_POCO_REGISTER);
    return -1;
  }

  /* prepare for positive edge */
  value = 0;
  if(write_pce(d, acc_ctl, &value, 0, 4) != 4){
    return -1;
  }
  /* now clear assorted status and error flags */
  value = VACC_DONE_POCO_FLAG | VACC_OF_POCO_FLAG | VACC_ERROR_POCO_FLAG | VACC_RST_POCO_FLAG;
  if(write_pce(d, acc_ctl, &value, 0, 4) != 4){
    return -1;
  }

  /* check accumulator status */
  if(read_name_pce(d, ACC_STATUS_POCO_REGISTER, &value, 0, 4) != 4){
    return -1;
  }
  if(value != VACC_READY_POCO_FLAG){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "value 0x%x not ready in %s", value, ACC_STATUS_POCO_REGISTER);
    return -1;
  }

  /* make accumulator go */
  value = VACC_EN_POCO_FLAG;
  if(write_pce(d, acc_ctl, &value, 0, 4) != 4){
    return -1;
  }
#endif

#if 0
  if(sp->p_manual){

    value = 0;
    if(write_pce(d, timing_ctl, &value, 0, 4) != 4){
      return -1;
    }
    value = TIMING_TRIGGER_FLAG;
    if(write_pce(d, timing_ctl, &value, 0, 4) != 4){
      return -1;
    }

    log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "triggering pps manually");

    gettimeofday(&now, NULL);

    if((USECPERSEC_POCO - now.tv_usec) < POCO_PAUSE_WINDOW){
      log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "preparation rather close to second boundary");
    }

    delay.tv_sec = 0;
    delay.tv_nsec = (USECPERSEC_POCO - now.tv_usec) * 1000;

    nanosleep(&delay, NULL);

    value = 0;
    if(write_pce(d, timing_ctl, &value, 0, 4) != 4){
      return -1;
    }
    value = TIMING_TRIGGER_FLAG;
    if(write_pce(d, timing_ctl, &value, 0, 4) != 4){
      return -1;
    }
  }
#endif

  return 0;
}
#endif

int compute_dump_poco(struct katcp_dispatch *d)
{
  unsigned int us;
  struct state_poco *sp;

  sp = get_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return -1;
  }

  if((sp->p_pre_accumulation <= 0) || (sp->p_fft_window <= 0)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "divisors are zero");
    return -1;
  }

  if(sp->p_accumulation_length <= 0){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "accumulation period was zero");
    sp->p_accumulation_length = 1;
  }

  sp->p_dump_count = (sp->p_accumulation_length * (sp->p_dsp_clock / 1000)) / (sp->p_pre_accumulation * sp->p_fft_window / 2);

  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "%u ms maps to %u periods", sp->p_accumulation_length, sp->p_dump_count);

  /* WARNING: the product below needs to fit within 32 bits */
  us = (sp->p_dump_count * sp->p_pre_accumulation * sp->p_fft_window / 2) / (sp->p_dsp_clock / 1000000);
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "accumulation length of %u adjusted to %u.%03u", sp->p_accumulation_length, us / 1000, us % 1000);

  return 0;
}

int do_sync_poco(struct katcp_dispatch *d)
{
  struct state_poco *sp;
  struct timeval now;
  struct timespec delay;
  unsigned int defer, retries;
  struct poco_core_entry *control, *control0;
  struct capture_poco *cp;
  uint32_t value;
  int i, wr, rr;

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "not in poco mode");
    return -1;
  }

  control = by_name_pce(d, CONTROL_POCO_REGISTER);
  control0 = by_name_pce(d, CONTROL0_POCO_REGISTER);

  if((control == NULL) || (control0 == NULL)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to locate register %s or %s", CONTROL_POCO_REGISTER, CONTROL0_POCO_REGISTER);
    return -1;
  }

  /* set dump rate */
  if(write_name_pce(d, ACC_LEN_POCO_REGISTER, &(sp->p_dump_count), 0, 4) != 4){
    return -1;
  }

  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "about to set sync");

  /* lower bits */
  value = sp->p_source_state;
  if(write_pce(d, control0, &value, 0, 4) != 4){
    return -1;
  }
 
  /* clear errors and select source */
  value = sp->p_source_state | IS_DONE_VACC_STATUS0 | IS_BUFFER_OVERFLOW_VACC_STATUS0 | IS_OVERWRITE_VACC_STATUS0 | IS_ACCUMULATOR_OVERFLOW_VACC_STATUS0;
  if(write_pce(d, control0, &value, 0, 4) != 4){
    return -1;
  }
 
  retries = POCO_RETRY_ATTEMPTS;
  defer = 0;

  do{
    if(defer > 0){
      delay.tv_sec = 0;
      delay.tv_nsec = defer * 1000;
      nanosleep(&delay, NULL);
    }

    gettimeofday(&now, NULL);

    if(now.tv_usec < (USECPERSEC_POCO / 4)){
      retries--;
      defer = (USECPERSEC_POCO / 4) - now.tv_usec;
    } else if(now.tv_usec > ( 2 * (USECPERSEC_POCO / 3))){
      retries--;
      defer = 7 * (USECPERSEC_POCO / 12);
    } else {
      defer = 0;
    } 
  } while((defer > 0) && (retries > 0));

  if(retries <= 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to enter safe window after %d attempts", POCO_RETRY_ATTEMPTS);
    return -1;
  }

  /* positive edge for arm */
  value = 0;
  wr = write_pce(d, control, &value, 0, 4);
  if(wr != 4){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to write 0x%08x to %s", value, CONTROL_POCO_REGISTER );
    return -1;
  }
  value = SYNC_ARM_CONTROL;
  wr = write_pce(d, control, &value, 0, 4);
  if(wr != 4){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to write 0x%08x to %s", value, CONTROL_POCO_REGISTER );
    return -1;
  }

  /* sanity check */
  rr = read_name_pce(d, STATUS_POCO_REGISTER, &value, 0, 4);
  if(rr != 4){
    return -1;
  }
  if(!(value & IS_RESET_STATUS)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "status isn't reset but 0x%08x", value);
    return -1;
  }

  /* figure out when sync is going to happen */
  sp->p_sync_time.tv_sec = now.tv_sec + 2;
  sp->p_sync_time.tv_usec = 0;

  if(sp->p_manual){
    /* make sync happen manually, bit bang 2 syncs */
    delay.tv_sec = 0;
    delay.tv_nsec = (USECPERSEC_POCO - now.tv_usec) * 1000;
    nanosleep(&delay, NULL);

    value = SYNC_MANUAL_CONTROL;
    wr = write_pce(d, control, &value, 0, 4);
    if(wr != 4){
      return -1;
    }
    value = 0;
    wr = write_pce(d, control, &value, 0, 4);
    if(wr != 4){
      return -1;
    }

    log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "generating 2 sync pulses manually");

    delay.tv_sec = 1;
    delay.tv_nsec = 0;
    nanosleep(&delay, NULL);

    value = SYNC_MANUAL_CONTROL;
    wr = write_pce(d, control, &value, 0, 4);
    if(wr != 4){
      return -1;
    }
  }

#if 0
  /* sanity check */
  rr = read_name_pce(d, PPS_PERIOD_POCO_REGISTER, &value, 0, 4);
  if(rr != 4){
    return -1;
  }

  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "sync at %lu.%06lus, pps at %u cycles", sp->p_sync_time.tv_sec, sp->p_sync_time.tv_usec, value);
#endif 

  /* clear all the sync counters */
  for(i = 0; i < sp->p_size; i++){
    cp = sp->p_captures[i];

    cp->c_ts_msw = 0;
    cp->c_ts_lsw = 0;
  }

  return 0;
}

int do_check_sync(struct katcp_dispatch *d)
{
  struct state_poco *sp;
  uint32_t min, max;

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "not in poco mode");
    return -1;
  }

  if(read_name_pce(d, PPS_PERIOD_MAX_POCO_REGISTER, &max, 0, 4) != 4){
    /* read functions should report their own errors */
    return -1;
  }

  if(read_name_pce(d, PPS_PERIOD_MIN_POCO_REGISTER, &min, 0, 4) != 4){
    /* read functions should report their own errors */
    return -1;
  }

  max++;
  min++;

  if((min == max) && (min == sp->p_dsp_clock)){
    log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "system clock nominal at %luHz", sp->p_dsp_clock);
    return 0;
  }

  if((min < POCO_DSP_CLOCK_TOL) && ((max + min) == sp->p_dsp_clock)){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "adc pps inputs misaligned by %d cycles", min - 1);
    return 0;
  }

  log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "system clock or pps unstable with minimum %dHz and maximum %dHz", min, max);
  return -1;

}

int do_fix_offset(struct katcp_dispatch *d)
{
  struct state_poco *sp;
  uint32_t count, offset;
  struct timeval now, up, mine, correct;
  struct timespec delay;
  struct poco_core_entry *pps_offset;
  unsigned int defer, retries;

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "not in poco mode");
    return -1;
  }

  pps_offset = by_name_pce(d, PPS_OFFSET_POCO_REGISTER);
  if(pps_offset == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "register %s not available", PPS_OFFSET_POCO_REGISTER);
    return -1;
  }

  do{
    if(defer > 0){
      delay.tv_sec = 0;
      /* WARNING: only works well for certain values of dsp_clock */
      delay.tv_nsec = defer * (NSECPERSEC_POCO / sp->p_dsp_clock);
      nanosleep(&delay, NULL);
    }

    if(read_pce(d, pps_offset, &offset, 0, 4) != 4){
      /* read functions report their own errors */
      return -1;
    }

    /* TODO: this is a clone of the sync logic, should be made generic */
    if(offset < (sp->p_dsp_clock / 4)){
      retries--;
      defer = (sp->p_dsp_clock / 4) - offset;
    } else if(offset > ( 2 * (sp->p_dsp_clock / 3))){
      retries--;
      defer = 7 * (sp->p_dsp_clock / 12);
    } else {
      defer = 0;
    } 
  } while((defer > 0) && (retries > 0));

  if(retries <= 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to enter safe gateware window after %d attempts", POCO_RETRY_ATTEMPTS);
    return -1;
  }

  gettimeofday(&now, NULL);

  if(read_name_pce(d, PPS_COUNT_POCO_REGISTER, &count, 0, 4) != 4){
    /* read functions report their own errors */
    return -1;
  }

  up.tv_sec = count;
  up.tv_usec = offset / (sp->p_dsp_clock / USECPERSEC_POCO);

  if(sub_time_katcp(&mine, &now, &(sp->p_sync_time)) < 0){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "detected time warp as sync time %lu.%06lus is in the future", sp->p_sync_time.tv_sec, sp->p_sync_time.tv_usec);
    return -1;
  }

  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "my time since sync is %lu.%06lus fpga says %lu.%06lus", mine.tv_sec, mine.tv_usec, up.tv_sec, up.tv_usec);

#if 0
  if(cmp_time_katcp(&now, &up) <= 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "clock unreasonable as system bootup time %lu.%06lus exceeds wall time", up.tv_sec, up.tv_usec);
    return -1;
  }

  sub_time_katcp(&correct, &now, &up);
#endif

  return -1;
}

int do_fft_poco(struct katcp_dispatch *d)
{
  struct state_poco *sp;
  uint32_t value;

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "not in poco mode");
    return -1;
  }

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "enabling fft shifts");
  value = 0xffffffff;
  if(write_name_pce(d, FFT_SHIFT_POCO_REGISTER, &value, 0, 4) != 4){
    return -1;
  }

  /* edge triggered */
  value = 0;
  if(write_name_pce(d, CONTROL_POCO_REGISTER, &value, 0, 4) != 4){
    return -1;
  }

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "clearing fft overflow flags");
  value = CLEAR_FFT_CONTROL(0xf);
  if(write_name_pce(d, CONTROL_POCO_REGISTER, &value, 0, 4) != 4){
    return -1;
  }

  return 0;
}

struct state_poco *ready_poco_poco(struct katcp_dispatch *d)
{
  struct state_poco *sp;
  void *ptr;

  sp = get_mode_katcp(d, POCO_POCO_MODE);
  ptr = get_current_mode_katcp(d);

  if((struct state_poco *)ptr == sp){
    return sp;
  }

  if(programmed_core_poco(d, sp->p_image)){
#ifdef DEBUG
    fprintf(stderr, "ready: still programmed\n");
#endif
    return sp;
  }

  return NULL;
}

int enter_mode_poco(struct katcp_dispatch *d, unsigned int from)
{
  struct state_poco *sp;

  sp = get_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return from;
  }

  if(program_core_poco(d, sp->p_image)){
    return from;
  }

  if(compute_dump_poco(d) < 0){
    return from;
  }

  if(do_sync_poco(d) < 0){
    return from;
  }

  if(do_fft_poco(d) < 0){
    return from;
  }

  return POCO_POCO_MODE;
}

void leave_mode_poco(struct katcp_dispatch *d, unsigned int to)
{
  struct state_poco *sp;

  sp = get_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return;
  }

  /* unclear how much should be stopped here: for certain cases one wants a snapshot crashed system, on the other hand things still have to be consistent */
}

void destroy_poco_poco(struct katcp_dispatch *d)
{
  struct state_poco *sp;
  int i;

  sp = get_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return;
  }

  if(sp->p_image){
    free(sp->p_image);
    sp->p_image = NULL;
  }

  for(i = 0; i < sp->p_size; i++){
    destroy_capture_poco(d, sp->p_captures[i]);
  }

  sp->p_size = 0;

  if(sp->p_captures){
    free(sp->p_captures);
    sp->p_captures = NULL;
  }

  for(i = 0; i < (POCO_ANT_COUNT * POCO_POLARISATION_COUNT); i++){
    shutdown_fixer(d, sp->p_fixers[i]);
    sp->p_fixers[i] = NULL;
  }

  free(sp);
}

struct capture_poco *find_capture_poco(struct state_poco *sp, char *name)
{
  int i;

  if(name == NULL){
    return NULL;
  }

  for(i = 0; i < sp->p_size; i++){
    if(!strcmp(sp->p_captures[i]->c_name, name)){
      return sp->p_captures[i];
    }
  }

#ifdef DEBUG
  fprintf(stderr, "find: unable to locate capture %s\n", name);
#endif

  return NULL;
}

int sync_now_cmd(struct katcp_dispatch *d, int argc)
{
  if(do_sync_poco(d) < 0){
    return KATCP_RESULT_FAIL;
  }

  return KATCP_RESULT_OK;
}

int sync_toggle_cmd(struct katcp_dispatch *d, int argc)
{
  struct state_poco *sp;

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return KATCP_RESULT_FAIL;
  }

  sp->p_manual = 1 - sp->p_manual;

  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "changed to %s sync mode", sp->p_manual ? "manual" : "pps");

  return KATCP_RESULT_OK;
}

/**********************************************************************/

int system_info_cmd(struct katcp_dispatch *d, int argc)
{
  struct state_poco *sp;

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return KATCP_RESULT_FAIL;
  }

  do_check_sync(d);
  do_fix_offset(d);

  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "%lu.%06lus last sync", sp->p_sync_time.tv_sec, sp->p_sync_time.tv_usec);
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "%uus lead time", sp->p_lead);
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "%s bof image", sp->p_image);
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "%d capture items", sp->p_size);
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "%u desired dsp clock", sp->p_dsp_clock);
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "%u fft window", sp->p_fft_window);
#if 0
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "%u pre accumulation", sp->p_pre_accumulation);
#endif
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "%u accumulation length (%u*%d)", sp->p_accumulation_length, sp->p_dump_count, sp->p_pre_accumulation);
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "%s sync mode", sp->p_manual ? "manual" : "pps");
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "%u sync period", sp->p_sync_period);

  dump_timers_katcp(d);

  return KATCP_RESULT_OK;
}

#if 0
int capture_poll_cmd(struct katcp_dispatch *d, int argc)
{
  struct state_poco *sp;
  struct capture_poco *cp;
  struct timeval period;
  char *name, *ptr;

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return KATCP_RESULT_FAIL;
  }

  if(argc <= 1){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "need parameters");
    extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
    return KATCP_RESULT_OWN;
  }

  name = arg_string_katcp(d, 1);
  cp = find_capture_poco(sp, name);
  if(cp == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "no match for name %s", name);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
    return KATCP_RESULT_OWN;
  }

  if(argc > 2){
    ptr = arg_string_katcp(d, 2);
    if(ptr == NULL){
      return KATCP_RESULT_FAIL;
    }

    if(time_from_string(&period, NULL, ptr) < 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "%s not a well-formed time", ptr);
      extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
      return KATCP_RESULT_OWN;
    }

    cp->c_period.tv_sec = period.tv_sec;
    cp->c_period.tv_usec = period.tv_usec;

  }

  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "poll period is %lu.%06lus", cp->c_period.tv_sec, cp->c_period.tv_usec);

  return KATCP_RESULT_OK;
}
#endif

/**********************************************************************/

int capture_destination_cmd(struct katcp_dispatch *d, int argc)
{
  struct state_poco *sp;
  struct capture_poco *cp;
  char *name, *host;
  unsigned long port;
  struct in_addr ina;
  struct hostent *he;

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return KATCP_RESULT_FAIL;
  }

  if(argc <= 3){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "need parameters");
    extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
    return KATCP_RESULT_OWN;
  }

  name = arg_string_katcp(d, 1);

  cp = find_capture_poco(sp, name);
  if(cp == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "no match for name %s", name);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
    return KATCP_RESULT_OWN;
  }

  host = arg_string_katcp(d, 2);
  if(inet_aton(host, &ina) == 0){
    he = gethostbyname(host);
    if((he == NULL) || (he->h_addrtype != AF_INET)){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to convert %s to ipv4 address", host);
      extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
      return KATCP_RESULT_OWN;
    }

    ina = *(struct in_addr *) he->h_addr;
  }

  cp->c_ip = ina.s_addr;

  port = arg_unsigned_long_katcp(d, 3);
  if((port <= 0) || (port > 0xffff)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "port %lu not in range", port);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
    return KATCP_RESULT_OWN;
  }

  cp->c_port = htons(port);

  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "destination is %08x:%04x", cp->c_ip, cp->c_port);

  if(init_udp_poco(d, cp)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to initiate udp connection");
    return KATCP_RESULT_FAIL;
  }

  return KATCP_RESULT_OK;
}

int capture_start_cmd(struct katcp_dispatch *d, int argc)
{
  struct state_poco *sp;
  struct capture_poco *cp;
  char *name, *ptr;
  struct timeval when, now, delta, prep, start, soonest;
  int result;

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return KATCP_RESULT_FAIL;
  }

  if(argc <= 1){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "need parameters");
    extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
    return KATCP_RESULT_OWN;
  }

  name = arg_string_katcp(d, 1);

  cp = find_capture_poco(sp, name);
  if(cp == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "no match for name %s", name);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
    return KATCP_RESULT_OWN;
  }

  delta.tv_sec = 0;
  delta.tv_usec = sp->p_lead;

  gettimeofday(&now, NULL);

  /* soonest prep time */
  add_time_katcp(&soonest, &now, &delta);

  if(cp->c_start.tv_sec != 0){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "changing previously scheduled start considered poor form");
  }

  if(argc <= 2){

    add_time_katcp(&start, &soonest, &delta);

    cp->c_prep.tv_sec  = soonest.tv_sec;
    cp->c_prep.tv_usec = soonest.tv_usec;

    cp->c_start.tv_sec  = start.tv_sec;
    cp->c_start.tv_usec = start.tv_usec;

  } else {
    ptr = arg_string_katcp(d, 2);
    if(ptr == NULL){
      return KATCP_RESULT_FAIL;
    }
    if(time_from_string(&when, NULL, ptr) < 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "%s not a well-formed time", ptr);
      extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
      return KATCP_RESULT_OWN;
    }

    if(sub_time_katcp(&prep, &when, &delta) < 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "%s is unreasonable", ptr);
      extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
      return KATCP_RESULT_OWN;
    }

    if(cmp_time_katcp(&prep, &(sp->p_sync_time)) < 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "clock unreasonable as sync time %lu.%06lus is in the future", sp->p_sync_time.tv_sec, sp->p_sync_time.tv_usec);
      extra_response_katcp(d, KATCP_RESULT_FAIL, "reasonability");
      return KATCP_RESULT_OWN;
    }

    if(cmp_time_katcp(&soonest, &prep) > 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "start time %s already passed or too soon");
      extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
      return KATCP_RESULT_OWN;
    }

    cp->c_prep.tv_sec  = prep.tv_sec;
    cp->c_prep.tv_usec = prep.tv_usec;

    cp->c_start.tv_sec  = when.tv_sec;
    cp->c_start.tv_usec = when.tv_usec;
  }

  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "preparing at %lu.%06lus and starting at %lu.%06lus", cp->c_prep.tv_sec, cp->c_prep.tv_usec, cp->c_start.tv_sec, cp->c_start.tv_usec);

#if 0
  if((cp->c_stop.tv_sec != 0) && (cmp_time_katcp(&(cp->c_stop), &(cp->c_start)) <= 0)){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "overriding stale stop carelessly scheduled for %lu.%06lus", cp->c_stop.tv_sec, cp->c_stop.tv_usec);
    cp->c_stop.tv_sec = 0;
    cp->c_stop.tv_usec = 0;
  }
#endif

  result = (*(cp->c_schedule))(d, cp, CAPTURE_POKE_START);
  if(result < 0){
    return KATCP_RESULT_FAIL;
  }

  return KATCP_RESULT_OK;
}

int capture_sync_cmd(struct katcp_dispatch *d, int argc)
{
  struct state_poco *sp;
  struct capture_poco *cp;
  char *name;

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return KATCP_RESULT_FAIL;
  }

  if(argc <= 1){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "need parameters");
    extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
    return KATCP_RESULT_OWN;
  }

  name = arg_string_katcp(d, 1);
  if(name == NULL){
    return KATCP_RESULT_FAIL;
  }

  cp = find_capture_poco(sp, name);
  if(cp == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "no match for name %s", name);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
    return KATCP_RESULT_OWN;
  }

#if 1
  /* somehow seems grotty */
  meta_udp_poco(d, cp, SYNC_STREAM_CONTROL_POCO);
  tx_udp_poco(d, cp);
#endif

  return KATCP_RESULT_OK;
}

int capture_stop_cmd(struct katcp_dispatch *d, int argc)
{
  struct state_poco *sp;
  struct capture_poco *cp;
  char *name, *ptr;
  struct timeval when;
  int result;

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return KATCP_RESULT_FAIL;
  }

  if(argc <= 1){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "need parameters");
    extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
    return KATCP_RESULT_OWN;
  }

  name = arg_string_katcp(d, 1);

  cp = find_capture_poco(sp, name);
  if(cp == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "no match for name %s", name);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
    return KATCP_RESULT_OWN;
  }

  if(cp->c_stop.tv_sec != 0){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "changing stop time while busy considered bad form");
  }

  if(argc <= 2){
    gettimeofday(&when, NULL);
  } else {
    ptr = arg_string_katcp(d, 2);
    if(ptr == NULL){
      return KATCP_RESULT_FAIL;
    }
    if(time_from_string(&when, NULL, ptr) < 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "%s not a well-formed time", ptr);
      extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
      return KATCP_RESULT_OWN;
    }
  }

#if 0
  /* if a start time is set and it is after the requested stop time, advance the stop time */
  if((cp->c_start.tv_sec != 0) && (cmp_time_katcp(&(cp->c_start), &when) >= 0)){
    /* move to the schedule function - it knows more */
  }
#endif

  cp->c_stop.tv_sec = when.tv_sec;
  cp->c_stop.tv_usec = when.tv_usec;

  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "shutdown request for %lu.%06lus", cp->c_stop.tv_sec, cp->c_stop.tv_usec);

  result = (*(cp->c_schedule))(d, cp, CAPTURE_POKE_STOP);
  if(result < 0){
    return KATCP_RESULT_FAIL;
  }

  return KATCP_RESULT_OK;
}

int capture_list_cmd(struct katcp_dispatch *d, int argc)
{
  struct state_poco *sp;
  struct capture_poco *cp;
  char *name, *ip;
  unsigned int count, i;
  struct in_addr in;

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return KATCP_RESULT_FAIL;
  }

  if(argc <= 1){
    name = NULL;
  } else {
    name = arg_string_katcp(d, 1);
  }

  count = 0;
  for(i = 0; i < sp->p_size; i++){
    if((name == NULL) || (!strcmp(sp->p_captures[i]->c_name, name))){
      cp = sp->p_captures[i];

      prepend_inform_katcp(d);
      append_string_katcp(d, KATCP_FLAG_STRING, cp->c_name);

      in.s_addr = cp->c_ip;
      ip = inet_ntoa(in);

      if(ip){
        append_string_katcp(d, KATCP_FLAG_STRING, ip);
      } else {
        append_hex_long_katcp(d, KATCP_FLAG_XLONG, (unsigned long) cp->c_ip);
      }

      append_unsigned_long_katcp(d, KATCP_FLAG_ULONG, (unsigned long) cp->c_port);

      if(cp->c_start.tv_sec){
        append_args_katcp(d, 0, "%lu%03lu", cp->c_start.tv_sec, cp->c_start.tv_usec / 1000);
      } else {
        append_unsigned_long_katcp(d, KATCP_FLAG_ULONG, 0UL);
      }

      if(cp->c_stop.tv_sec){
        append_args_katcp(d, KATCP_FLAG_LAST, "%lu%03lu", cp->c_stop.tv_sec, cp->c_stop.tv_usec / 1000);
      } else {
        append_unsigned_long_katcp(d, KATCP_FLAG_LAST | KATCP_FLAG_ULONG, 0UL);
      }

#if 0
      send_katcp(d, KATCP_FLAG_FIRST | KATCP_FLAG_STRING, "#capture-list", 
                                       KATCP_FLAG_STRING, cp->c_name,
                                       KATCP_FLAG_XLONG,  (unsigned long) cp->c_ip, 
                    KATCP_FLAG_LAST |  KATCP_FLAG_ULONG,  (unsigned long) cp->c_port
                    );
#endif

      log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "start=%lu.%06lus stop=%lu.%06lus state=%d", cp->c_start.tv_sec, cp->c_start.tv_usec, cp->c_stop.tv_sec, cp->c_stop.tv_usec, cp->c_state);

      count++;
    }
  }

  if(count > 0){
    send_katcp(d, KATCP_FLAG_FIRST | KATCP_FLAG_STRING, "!capture-list", 
                  KATCP_FLAG_STRING,                    KATCP_OK,
                  KATCP_FLAG_LAST  | KATCP_FLAG_ULONG, (unsigned long) count);
  } else {
    if(name){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "no match for capture with name %s", name);
    } /* else rather odd error */
    extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
  }

  return KATCP_RESULT_OWN;
}

/****************************************************************************/

int poco_delay_cmd(struct katcp_dispatch *d, int argc)
{
  char *input, *ptr;
  struct fixer_poco *fp;
  struct state_poco *sp;
  struct timeval when_tv;
  int delay_cycles, rate_delta, result, ant, from, to, pol, i;
  unsigned int rate_add, when_extra;
  long delay_time, rate_time;
  unsigned long adc;
  uint64_t when_cycles;

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return KATCP_RESULT_FAIL;
  }

#ifdef DEBUG
  fp = NULL;
#endif

  adc = sp->p_dsp_clock * POCO_ADC_DSP_FACTOR;

  if(argc < 4){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "need channel time delay and optional rate as parameter");
    extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
    return KATCP_RESULT_OWN;
  }

  input = arg_string_katcp(d, 1); /* input */
  if(input == NULL){
    return KATCP_RESULT_FAIL;
  }
  ant = extract_ant_poco(input);
  if(ant < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to extract antenna from %s", input);
    return KATCP_RESULT_FAIL;
  }
  pol = extract_polarisation_poco(input);
  if(pol < 0){
    from = ant * POCO_POLARISATION_COUNT;
    to = from + POCO_POLARISATION_COUNT;
  } else {
    from = (ant * POCO_POLARISATION_COUNT) + pol;
    to = from + 1;
  }
  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "inputs from %d to %d", from, to);

  ptr = arg_string_katcp(d, 2); /* time */
  if(ptr == NULL){
    return KATCP_RESULT_FAIL;
  }

  if(time_from_string(&when_tv, &when_extra, ptr) < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "%s not a well-formed time", ptr);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
    return KATCP_RESULT_OWN;
  }

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "%s maps to %lu.%06lus + %lufs", ptr, when_tv.tv_sec, when_tv.tv_usec, when_extra);

  if((when_tv.tv_sec == 0) && (when_tv.tv_usec == 0) && (when_extra == 0)){
    gettimeofday(&when_tv, NULL);
    when_tv.tv_sec++;
    when_extra = 0;
  }

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "using start time %lu.%06lus", when_tv.tv_sec, when_tv.tv_usec);

  result = tve_to_mcount_poco(d, &when_cycles, &when_tv, when_extra);
  if(result){
    if(result < 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "given time before last sync");
      return KATCP_RESULT_FAIL;
    } else {
      log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "start time has greater resolution that adc clock");
    }
  }
  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "mcount when is %llu", when_cycles);

  ptr = arg_string_katcp(d, 3); /* delay */
  if(ptr == NULL){
    return KATCP_RESULT_FAIL;
  }
  result = shift_point_string(&delay_time, ptr, 8);
  if(result < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "delay value is too large or otherwise invalid");
    extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
    return KATCP_RESULT_OWN;
  } 
  if(result > 0){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "discarding excess delay precision (limited to 1/%luHz)", adc);
  }
  if(delay_time < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "negative delays not reasonable");
    return KATCP_RESULT_FAIL;
  }
  delay_cycles = (delay_time * (adc / 1000000)) / 100000;
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "delay of %lu0ps maps to %d samples at %dHz", delay_time, delay_cycles, adc);
  if(abs(delay_cycles) > POCO_MAX_COARSE_DELAY){
    log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "requested delay exceeds maximum of %dsamples at %luHz", POCO_MAX_COARSE_DELAY, adc);
    return KATCP_RESULT_FAIL;
  }

  if(argc > 4){ /* optional rate */
    ptr = arg_string_katcp(d, 4);
    if(ptr == NULL){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to acquire rate parameter");
      return KATCP_RESULT_FAIL;
    }
    result = shift_point_string(&rate_time, ptr, 15); /* femto */
    if(result < 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "rate not in range");
      return KATCP_RESULT_FAIL;
    }
    if(result > 0){
      log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "discarding excess rate precision");
    }
  } else {
    rate_time = 0;
  }
  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "rate of %ldfs/s", rate_time);

  if(rate_time == 0){
    rate_delta = 0;
    rate_add = 0;
  } else {
    /* currently only adjust by single steps */

    rate_delta = (rate_time < 0) ? (-1) : 1;
    /* make rate_add positive no matter what sign_rate time */
    rate_add = rate_delta * 1000000000 / (rate_time * (adc / USECPERSEC_POCO));

    if(rate_add <= 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "update rate too fast (%ld * %lu)", rate_time, adc / USECPERSEC_POCO);
      return -1;
    }
  }

  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "need to update by %d every %ld seconds", rate_delta, rate_add);

  for(i = from; i < to; i++){
    fp = sp->p_fixers[i];

    fp->f_stage_mcount = when_cycles;
    fp->f_stage_delay = delay_cycles;

    fp->f_stage_add = rate_add;
    fp->f_stage_delta = rate_delta;

#if 0
    log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "scheduling fixer[%d] for %lu.%06lus", i, headstart.tv_sec, headstart.tv_usec);
#endif

    schedule_fixer(d, fp);
  }
   
  return KATCP_RESULT_OK;
}

/* poco_gain() in gain.c */

int poco_accumulation_length_cmd(struct katcp_dispatch *d, int argc)
{
  struct state_poco *sp;
  struct timeval interval;
  char *ptr;

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return KATCP_RESULT_FAIL;
  }

  if(argc <= 1){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "need parameters");
    extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
    return KATCP_RESULT_OWN;
  }

  ptr = arg_string_katcp(d, 1);
  if(ptr == NULL){
    return KATCP_RESULT_FAIL;
  }

  if(time_from_string(&interval, NULL, ptr) < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "%s not a well-formed time", ptr);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
    return KATCP_RESULT_OWN;
  }

  if(interval.tv_sec > 10){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "integrations longer than 10000ms not supported");
  }

  /* looks embarrassing */
  sp->p_accumulation_length = (interval.tv_sec * 1000) + (interval.tv_usec / 1000);

  if(compute_dump_poco(d) < 0){
    return KATCP_RESULT_FAIL;
  }

  return KATCP_RESULT_OK;
}

int poco_phase_cmd(struct katcp_dispatch *d, int argc)
{
  char *input, *phase_string;
  int ant, from, to, pol, result, i;
  struct fixer_poco *fp;
  struct state_poco *sp;
  long phase;

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return KATCP_RESULT_FAIL;
  }

  if(argc < 2){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "need input and optionally phase");
    extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
    return KATCP_RESULT_OWN;
  }

  input = arg_string_katcp(d, 1); /* input */
  if(input == NULL){
    return KATCP_RESULT_FAIL;
  }
  ant = extract_ant_poco(input);
  if(ant < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to extract antenna from %s", input);
    return KATCP_RESULT_FAIL;
  }
  pol = extract_polarisation_poco(input);
  if(pol < 0){
    from = ant * POCO_POLARISATION_COUNT;
    to = from + POCO_POLARISATION_COUNT;
  } else {
    from = (ant * POCO_POLARISATION_COUNT) + pol;
    to = from + 1;
  }

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "inputs from %d to %d", from, to);

  if(argc > 2){
    phase_string = arg_string_katcp(d, 2); /* phase constant */
    if(phase_string == NULL){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to acquire phase parameter");
      return KATCP_RESULT_FAIL;
    }

    result = shift_point_string(&phase, phase_string, 3);
    if(result < 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "phase value is too large or otherwise invalid");
      extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
      return KATCP_RESULT_OWN;
    } 
    if(result > 0){
      log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "discarding excess phase precision");
    }

    for(i = from; i < to; i++){
      fp = sp->p_fixers[i];

      log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "setting fixer %d from %d to %ldmr", i, fp->f_phase_mr, phase);

      fp->f_phase_mr = phase;
    }
  }

  prepend_reply_katcp(d);
  append_string_katcp(d, KATCP_FLAG_STRING, KATCP_OK);

  i = from;

  while(i < to){
    fp = sp->p_fixers[i];
    i++;

    /* @%#$%! - % can do negatives ... */
    append_args_katcp(d, (i == to) ? KATCP_FLAG_LAST : 0, "%d.%d", fp->f_phase_mr / 1000, 0);
  }

  return KATCP_RESULT_OWN;
#if 0
  return KATCP_RESULT_OK;
#endif
}

int poco_lo_freq_cmd(struct katcp_dispatch *d, int argc)
{
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "local oscillator not used");

  return KATCP_RESULT_OK;
}

int poco_tau_max_cmd(struct katcp_dispatch *d, int argc)
{
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "tau max not used");

  return KATCP_RESULT_OK;
}

/*****************************************************************************/

int setup_poco_poco(struct katcp_dispatch *d, char *image)
{
  struct state_poco *sp;
  struct katcp_acquire *a;
  int result, i;

  if(image == NULL){
    return -1;
  }

  sp = malloc(sizeof(struct state_poco));
  if(sp == NULL){
    return -1;
  }

  sp->p_image = NULL;

  sp->p_captures = NULL;
  sp->p_size = 0;

  sp->p_sync_time.tv_sec = 0;
  sp->p_sync_time.tv_usec = 0;

  sp->p_lead = POCO_DEFAULT_LEAD;

  for(i = 0; i < (POCO_ANT_COUNT * POCO_POLARISATION_COUNT); i++){
    sp->p_fixers[i] = NULL;
  }

  sp->p_dsp_clock = POCO_DSP_CLOCK;
  sp->p_fft_window = POCO_FFT_WINDOW;
  sp->p_pre_accumulation = POCO_PRE_ACCUMULATION;

  sp->p_accumulation_length = POCO_DEFAULT_ACCUMULATION;
  sp->p_dump_count = 1;
  sp->p_sync_period = POCO_DEFAULT_SYNC_PERIOD;
  sp->p_source_state = SOURCE_SELECT_VACC_CONTROL0;

  /* mcount unset */

  sp->p_manual = 0;

  /* gateware overflow check */
  sp->p_overflow_sensor.a_status = NULL;
  sp->p_overflow_sensor.a_control = NULL;
  
  /* psu/system stuff */
  for(i = 0; i < HWMON_COUNT_POCO; i++){
    sp->p_hwmon_sensor[i].h_fd = (-1);
  }

  /* clock sync */
  sp->p_ntp_sensor.n_fd = (-1); 

  sp->p_image = strdup(image);
  if(sp->p_image == NULL){
    destroy_poco_poco(d);
    return -1;
  }

  if(store_full_mode_katcp(d, POCO_POCO_MODE, POCO_POCO_NAME, &enter_mode_poco, NULL, sp, &destroy_poco_poco) < 0){
    fprintf(stderr, "setup: unable to register poco mode\n");
    destroy_poco_poco(d);
    return -1;
  }

  result = 0;

#if 0
  result += register_snap_poco(d, "gaintest", "adc0_pol0_fft_snap", XXX, 4);
  result += register_snap_poco(d, "snap", "snap", 2048, 1);
#endif

  result += register_dram_poco(d, "dram", POCO_FFT_WINDOW * 12); /* is it really FFT WINDOW */

  if(result < 0){
    fprintf(stderr, "setup: unable to register capture instances\n");
    return -1;
  }

  result = 0;

  result += setup_hwmon_poco(d, &(sp->p_hwmon_sensor[0]), "poco.psu.3v3aux", "3.3v auxilliary powersupply good", "/sys/class/i2c-adapter/i2c-0/0-000f/power1_input", 1, 1);
  result += setup_hwmon_poco(d, &(sp->p_hwmon_sensor[1]), "poco.psu.mgtpll", "MGTPLL powersupply good", "/sys/class/i2c-adapter/i2c-0/0-000f/power2_input", 1, 1);
  result += setup_hwmon_poco(d, &(sp->p_hwmon_sensor[2]), "poco.psu.mgtvtt", "MGTVTT powersupply good", "/sys/class/i2c-adapter/i2c-0/0-000f/power3_input", 1, 1);
  result += setup_hwmon_poco(d, &(sp->p_hwmon_sensor[3]), "poco.psu.mgtvcc", "MGTVCC powersupply good", "/sys/class/i2c-adapter/i2c-0/0-000f/power4_input", 1, 1);

  if(result < 0){
    fprintf(stderr, "setup: unable to register hardware sensors\n");
    return -1;
  }
  
  a = setup_integer_acquire_katcp(d, &acquire_overflow_poco, &(sp->p_overflow_sensor));
  if(a == NULL){
    fprintf(stderr, "setup: unable to allocate adc acquisition\n");
    return -1;
  }

  result += register_multi_boolean_sensor_katcp(d, POCO_POCO_MODE, "poco.input.0x.overflow", "overflow indicator", "none", a, &extract_adc_poco);
  result += register_multi_boolean_sensor_katcp(d, POCO_POCO_MODE, "poco.input.0y.overflow", "overflow indicator", "none", a, &extract_adc_poco);
  result += register_multi_boolean_sensor_katcp(d, POCO_POCO_MODE, "poco.input.1x.overflow", "overflow indicator", "none", a, &extract_adc_poco);
  result += register_multi_boolean_sensor_katcp(d, POCO_POCO_MODE, "poco.input.1y.overflow", "overflow indicator", "none", a, &extract_adc_poco);

  result += register_multi_boolean_sensor_katcp(d, POCO_POCO_MODE, "poco.quant.0x.overflow", "overflow indicator", "none", a, &extract_quant_poco);
  result += register_multi_boolean_sensor_katcp(d, POCO_POCO_MODE, "poco.quant.0y.overflow", "overflow indicator", "none", a, &extract_quant_poco);
  result += register_multi_boolean_sensor_katcp(d, POCO_POCO_MODE, "poco.quant.1x.overflow", "overflow indicator", "none", a, &extract_quant_poco);
  result += register_multi_boolean_sensor_katcp(d, POCO_POCO_MODE, "poco.quant.1y.overflow", "overflow indicator", "none", a, &extract_quant_poco);

  result += register_multi_boolean_sensor_katcp(d, POCO_POCO_MODE, "poco.delay.0x.armed", "check if scheduled", "none", a, &extract_delay_poco);
  result += register_multi_boolean_sensor_katcp(d, POCO_POCO_MODE, "poco.delay.0y.armed", "check if scheduled", "none", a, &extract_delay_poco);
  result += register_multi_boolean_sensor_katcp(d, POCO_POCO_MODE, "poco.delay.1x.armed", "check if scheduled", "none", a, &extract_delay_poco);
  result += register_multi_boolean_sensor_katcp(d, POCO_POCO_MODE, "poco.delay.1y.armed", "check if scheduled", "none", a, &extract_delay_poco);

  if(result < 0){
    fprintf(stderr, "setup: unable to allocate adc sensors\n");
    destroy_acquire_katcp(d, a);
    return -1;
  }

  if(init_ntp_poco(d, &(sp->p_ntp_sensor)) < 0){
    fprintf(stderr, "setup: unable to initialise ntp sensor\n");
    return -1;
  }

  if(register_boolean_sensor_katcp(d, POCO_POCO_MODE, "poco.timing.sync", "clock good", "none", &acquire_ntp_poco, &(sp->p_ntp_sensor)) < 0){
    fprintf(stderr, "setup: unable to allocate ntp sensor\n");
    destroy_acquire_katcp(d, a);
    return -1;
  }

  for(i = 0; i < (POCO_ANT_COUNT * POCO_POLARISATION_COUNT); i++){
    sp->p_fixers[i] = startup_fixer(d, i);
    if(sp->p_fixers[i] == NULL){
      fprintf(stderr, "setup: unable to set up fixer instances");
      return -1;
    }
  }

  /* capture commands */
  result += register_mode_katcp(d, "?capture-list", "lists capture instances (?capture-list)", &capture_list_cmd, POCO_POCO_MODE);
  result += register_mode_katcp(d, "?capture-destination", "sets the network destination (?capture-destination name ip port)", &capture_destination_cmd, POCO_POCO_MODE);
  result += register_mode_katcp(d, "?capture-start", "start a capture (?capture-start name [time])", &capture_start_cmd, POCO_POCO_MODE);
  result += register_mode_katcp(d, "?capture-stop", "stop a capture (?capture-stop name [time])", &capture_stop_cmd, POCO_POCO_MODE);
  result += register_mode_katcp(d, "?capture-sync", "emit header for a capture stream (?capture-sync name)", &capture_sync_cmd, POCO_POCO_MODE);

  /* parameters */
  result += register_mode_katcp(d, "?poco-accumulation-length", "sets the accumulation length (?poco-accumlation-length accumulation-period)", &poco_accumulation_length_cmd, POCO_POCO_MODE);
  result += register_mode_katcp(d, "?poco-gain", "sets the digital gain (?poco-gain ([antenna[polarisation[channe]] value)+) (?poco-gain (value):sets all inp,pol,chn with gain value)", &poco_gain_cmd, POCO_POCO_MODE);
  result += register_mode_katcp(d, "?poco-delay", "sets the coarse delay (?poco-delay antenna[polarisation] time delay rate)", &poco_delay_cmd, POCO_POCO_MODE);

  result += register_mode_katcp(d, "?poco-snap-shot", "grabs a snap shot", &poco_snap_shot_cmd, POCO_POCO_MODE);

  /* not yet available */
  result += register_mode_katcp(d, "?poco-phase", "sets the fine delay (unimplemented)", &poco_phase_cmd, POCO_POCO_MODE);
  result += register_mode_katcp(d, "?poco-lo-freq", "sets the local oscillator frequency (unimplemented)", &poco_lo_freq_cmd, POCO_POCO_MODE);
  result += register_mode_katcp(d, "?poco-tau-max", "sets the total delay (unimplemented)", &poco_tau_max_cmd, POCO_POCO_MODE);

  /* commands outside ICD */
  result += register_mode_katcp(d, "?system-info", "display various pieces of system information (?system-info)", &system_info_cmd, POCO_POCO_MODE);
  result += register_mode_katcp(d, "?sync-toggle", "changes sync mode (?sync-toggle)", &sync_toggle_cmd, POCO_POCO_MODE);
  result += register_mode_katcp(d, "?sync-now", "resync the system (?sync-now)", &sync_now_cmd, POCO_POCO_MODE);

#if 0
  result += register_mode_katcp(d, "?capture-poll", "set the poll interval for a capture stream (?capture-poll name [period])", &capture_poll_cmd, POCO_POCO_MODE);
#endif

  if(result < 0){
    fprintf(stderr, "setup: unable to register poco mode commands\n");
    return -1;
  }

  return 0;
}
