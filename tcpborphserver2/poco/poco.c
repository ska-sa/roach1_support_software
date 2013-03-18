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
#include "input.h"

/* needed for meta packet, unclear if we should dive into that here */
#include "options.h"

#ifdef HACK
#include "raw.h"
#endif

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

#if 0
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

  /* clear all the sync counters */
  for(i = 0; i < sp->p_size; i++){
    cp = sp->p_captures[i];

    cp->c_ts_msw = 0;
    cp->c_ts_lsw = 0;
  }

  return 0;
}
#endif

int sync_gateware_poco(struct katcp_dispatch *d)
{
  struct state_poco *sp;
  struct timeval now;
  struct timespec delay;
  unsigned int retries, cpu, fpga, inc;
  struct poco_core_entry *control, *control0, *offset;
  struct capture_poco *cp;
  uint32_t value, fraction;
  int i, wr, rr;

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "not in poco mode");
    return -1;
  }

#define fetch_register(v, n)     if(((v) = by_name_pce(d, (n))) == NULL) { log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to locate register %s", (n)); return -1; }

  fetch_register(control,  CONTROL_POCO_REGISTER)
  fetch_register(control0, CONTROL0_POCO_REGISTER)
  fetch_register(offset,   PPS_OFFSET_POCO_REGISTER)

#undef fetch_register

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

  do{

    if(read_pce(d, offset, &fraction, 0, 4) != 4){
      return -1;
    }

    if(fraction > sp->p_dsp_clock){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "%u exceeeds reasonable number of cycles", fraction);
      return -1;
    }

    if(fraction > sp->p_dsp_clock / 2){
      /* WARNING: assumes dsp_clock to be a clean factor of a billion */

      delay.tv_sec = 0;
      delay.tv_nsec = (sp->p_dsp_clock - fraction) * (NSECPERSEC_POCO / sp->p_dsp_clock) + sp->p_lead; /* here the term lead this is a misnomer, it is more of a margin */

      nanosleep(&delay, NULL);

      retries--;

      if(retries <= 0){
        log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to enter safe window after %d attempts", POCO_RETRY_ATTEMPTS);
        return -1;
      }

    } else {
      gettimeofday(&now, NULL);

      retries = 0;
    }
  } while (retries > 0);

  /* WARNING: now gettimeofday(now) has to have run */

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

  fpga = fraction / (sp->p_dsp_clock / USECPERSEC_POCO);
  cpu = now.tv_usec;

  /* gateware waits for two pps to start counting */

  /* if the cpu is ahead of the fpga, but by no more than a third of a second, 
   * we assume that cpu leads, otherwise we assume a lag, so if cpu is ahead
   * by more than a third, we make it catch up another second */

  inc = (fpga + (USECPERSEC_POCO / 3) < cpu) ? 3 : 2;

  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "fpga at %uus and cpu at %uus thus incrementing sync time by %d", fpga, cpu, inc);

  sp->p_sync_time.tv_sec = now.tv_sec + inc;
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

    log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "generating sync pulses manually");

    delay.tv_sec = 1;
    delay.tv_nsec = 0;
    nanosleep(&delay, NULL);

    value = SYNC_MANUAL_CONTROL;
    wr = write_pce(d, control, &value, 0, 4);
    if(wr != 4){
      return -1;
    }
  } else {
    delay.tv_sec = 1;
    delay.tv_nsec = (USECPERSEC_POCO - now.tv_usec) * 1000;

    log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "pausing for 1.%06lus while gateware resets", USECPERSEC_POCO - now.tv_usec);

    nanosleep(&delay, NULL);
  }

  /* clear all the sync counters */
  for(i = 0; i < sp->p_size; i++){
    cp = sp->p_captures[i];

    cp->c_ts_msw = 0;
    cp->c_ts_lsw = 0;
  }

  return 0;
}

int check_read_performance(struct katcp_dispatch *d)
{
  struct state_poco *sp;
  uint32_t values[POCO_BENCHMARK_TRIES];
  char *names[] = { "name", "cache" };
  unsigned int sum, i, delta, min, max, valid, k; 
  struct poco_core_entry *pps_offset;

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "not in poco mode");
    return -1;
  }

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "not in poco mode");
    return -1;
  }

  max = min = 0; /* pacify gcc */

  for(k = 0; k < 2; k++){

    switch(k){
      case 0 : /* slow test, doing a name lookup */
        for(i = 0; i < POCO_BENCHMARK_TRIES; i++){
          read_name_pce(d, PPS_OFFSET_POCO_REGISTER, &(values[i]), 0, 4);
        }
        break;
      case 1 : /* fast, doing caching */
        pps_offset = by_name_pce(d, PPS_OFFSET_POCO_REGISTER);
        if(pps_offset){
          for(i = 0; i < POCO_BENCHMARK_TRIES; i++){
            read_pce(d, pps_offset, &(values[i]), 0, 4);
          }
        }
        break;
    }

    valid = 0;
    sum = 0;
    for(i = 0; i < (POCO_BENCHMARK_TRIES - 1); i++){
      if(values[i] < values[i + 1]){
        delta = values[i + 1] - values[i];
        if(i > 0){
          if(delta > max) max = delta;
          if(delta < min) min = delta;
        } else {
          min = delta;
          max = delta;
        }
        sum += delta;
        valid++;
      }
    }

    if(valid){
#define NS_SCALE (NSECPERSEC_POCO / POCO_DSP_CLOCK)
      log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "%s test best latency is %uns and worst is %uns with an average of %uns", names[k], min * NS_SCALE, max * NS_SCALE, (sum / valid) * NS_SCALE);
#undef NS_SCALE
    }
  }

  return 0;
}

int check_pps_poco(struct katcp_dispatch *d)
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

  log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "fpga clock or pps unstable with minimum %dHz and maximum %dHz", min, max);
  return -1;

}

int check_time_poco(struct katcp_dispatch *d, int info)
{
  struct timeval cpu, fpga, delta, report, tolerance;
  struct poco_core_entry *count, *offset;
  int result, tries, sign;
  uint32_t ticks, fraction, wrap;
  struct state_poco *sp;

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "not in poco mode");
    return -1;
  }

#if 0
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

  if((min != max) || (min != sp->p_dsp_clock)){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "fpga clock unstable between %luHz and %luHz", min, max);
  }
#endif

  if(sp->p_sync_time.tv_sec == 0){
    log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "system not synced");
    return -1;
  }

  count = by_name_pce(d, PPS_COUNT_POCO_REGISTER);
  offset = by_name_pce(d, PPS_OFFSET_POCO_REGISTER);

  if((count == NULL) || (offset == NULL)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to locate register %s or %s", PPS_COUNT_POCO_REGISTER, PPS_OFFSET_POCO_REGISTER);
    return -1;
  }

  tries = POCO_TIME_CHECK_TRIES;

  while(tries > 0){
    result = 0; 
    if(read_pce(d, offset, &wrap, 0, 4) != 4) { result = (-1); }

    if(read_pce(d, count, &ticks, 0, 4) != 4) { result = (-1); }
    if(read_pce(d, offset, &fraction, 0, 4) != 4) { result = (-1); }

    gettimeofday(&cpu, NULL);

    if(result == 0){
      if(fraction > wrap){

        report.tv_sec = ticks;
        /* WARNING: assumes fpga clock to be a clean MHz multiple */
        report.tv_usec = fraction / (sp->p_dsp_clock / USECPERSEC_POCO);

        if(cmp_time_katcp(&(sp->p_sync_time), &cpu) >= 0){
          log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unreasonable sync time %lu.%06lus is in the future", sp->p_sync_time.tv_sec, sp->p_sync_time.tv_usec);
          return -1;
        }

        add_time_katcp(&fpga, &(sp->p_sync_time), &report);

        if(info){
          log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "cpu time %lu.%06lus",  cpu.tv_sec,  cpu.tv_usec);
          log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "fpga time %lu.%06lus", fpga.tv_sec, fpga.tv_usec);
        }

        sign = cmp_time_katcp(&fpga, &cpu);
        if(sign == 0){
          /* this is almost too good to be true */
          if(info){
            log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "fpga and cpu time remarkably well aligned");
          }
          return 0;
        }

        if(sign < 0){
          sub_time_katcp(&delta, &cpu, &fpga);
        } else {
          sub_time_katcp(&delta, &fpga, &cpu);
        }

        tolerance.tv_sec = 0;
        tolerance.tv_usec = POCO_TIME_TOLERANCE;

        if(cmp_time_katcp(&delta, &tolerance) > 0){
          log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unreasonable %s lag of %lu.%06lus", (sign < 0) ? "fpga" : "cpu", delta.tv_sec, delta.tv_usec);
          return -1;
        }

        if(info){
          /* within tolerable range */
          log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "%s lags by %lu.%06lus", (sign < 0) ? "fpga" : "cpu", delta.tv_sec, delta.tv_usec);
        }

        return 0;
      }
    }

    tries--;
  }

  log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to acquire time after %d attempts", POCO_TIME_CHECK_TRIES);

  return -1;
}

static int fixup_time_poco(struct katcp_dispatch *d)
{
  struct timeval cpu, sync, up;
  struct poco_core_entry *count, *offset;
  int result, tries, sign;
  unsigned int delta;
  uint32_t ticks, fraction, wrap;
  struct state_poco *sp;

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "not in poco mode");
    return -1;
  }

  count = by_name_pce(d, PPS_COUNT_POCO_REGISTER);
  offset = by_name_pce(d, PPS_OFFSET_POCO_REGISTER);

  if((count == NULL) || (offset == NULL)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to locate register %s or %s", PPS_COUNT_POCO_REGISTER, PPS_OFFSET_POCO_REGISTER);
    return -1;
  }

  tries = POCO_TIME_CHECK_TRIES;

  while(tries > 0){
    result = 0; 
    if(read_pce(d, offset, &wrap, 0, 4) != 4) { result = (-1); }

    if(read_pce(d, count, &ticks, 0, 4) != 4) { result = (-1); }
    if(read_pce(d, offset, &fraction, 0, 4) != 4) { result = (-1); }

    gettimeofday(&cpu, NULL);

    if(result == 0){
      if(fraction > wrap){

        up.tv_sec = ticks;
        /* WARNING: assumes fpga clock to be a clean MHz multiple */
        up.tv_usec = fraction / (sp->p_dsp_clock / USECPERSEC_POCO);

        if(cmp_time_katcp(&cpu, &up) <= 0){
          log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "fpga reports uptime %lu.%06lus larger than unix time", up.tv_sec, up.tv_usec);
          return -1;
        }

        /* figure out when sync happend, given current cpu time and fpga uptime */
        sub_time_katcp(&sync, &cpu, &up);

        if(sync.tv_usec > (USECPERSEC_POCO / 2)){
          sync.tv_sec = sync.tv_sec + 1;
          delta = USECPERSEC_POCO - sync.tv_usec;
          sign = -1; /* cpu lags fpga */
        } else {
          delta = sync.tv_usec;
          sign = 1; /* cpu leads fpga */
        }

        if(sync.tv_sec != sp->p_sync_time.tv_sec){
          log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "updating sync time from %lu to %lu", sp->p_sync_time.tv_sec, sync.tv_sec);

          sp->p_sync_time.tv_sec = sync.tv_sec;
          sp->p_sync_time.tv_usec = 0;
        }

        if(delta > POCO_TIME_TOLERANCE){
          log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "cpu %s fpga by a worrying %uus", (sign > 0) ? "leads" : "lags", delta);
          return -1;
        }


        return 0;
      }
    }

    tries--;
  }

  log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to acquire time after %d attempts", POCO_TIME_CHECK_TRIES);

  return -1;
}

#if 0
int do_fix_offset(struct katcp_dispatch *d)
{
  struct state_poco *sp;
  uint32_t count, offset;
  struct timeval now, up, mine;
#if 0
  struct timeval correct;
#endif
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

  defer = 0;
  retries = POCO_RETRY_ATTEMPTS;

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
#endif

static int load_fft_poco(struct katcp_dispatch *d)
{
  struct state_poco *sp;
  uint32_t value;

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "not in poco mode");
    return -1;
  }

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "enabling fft shifts");
  value = sp->p_shift;
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

int enter_mode_poco(struct katcp_dispatch *d, char *flags, unsigned int from)
{
  struct state_poco *sp;

  sp = get_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return from;
  }

  if(program_core_poco(d, sp->p_image) < 0){
    return from;
  }

  if(compute_dump_poco(d) < 0){
    return from;
  }

  if(sync_gateware_poco(d) < 0){
    return from;
  }

  if(load_fft_poco(d) < 0){
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
  if(sync_gateware_poco(d) < 0){
    return KATCP_RESULT_FAIL;
  }

  return KATCP_RESULT_OK;
}

int option_set_cmd(struct katcp_dispatch *d, int argc)
{
  struct state_poco *sp;
  char *string;
  int i;

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return KATCP_RESULT_FAIL;
  }

  for(i = 1; i < argc; i++){
    string = arg_string_katcp(d, 1);
    if(string){
      if(!strcmp(string, "manual")){
        sp->p_manual = 1;
      } else if(!strcmp(string, "pps")){
        sp->p_manual = 0;
      } else if(!strcmp(string, "initial")){
        sp->p_continuous = 0;
      } else if(!strcmp(string, "continuous")){
        sp->p_continuous = 1;
      } else {
        log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "unknown option %s", string);
        return KATCP_RESULT_FAIL;
      }
    }
  }

  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "%s sync mode", sp->p_manual ? "manual" : "pps");
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "%s sync time updates", sp->p_continuous ? "continuous" : "initial");

  return KATCP_RESULT_OK;
}

/**********************************************************************/

int system_lead_cmd(struct katcp_dispatch *d, int argc)
{
  struct state_poco *sp;
  unsigned int lead;

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return KATCP_RESULT_FAIL;
  }

  if(argc > 1){
    lead = arg_unsigned_long_katcp(d, 1);
    if(lead <= 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "lead time not valid");
      return KATCP_RESULT_FAIL;
    }
    sp->p_lead = lead;
  }

  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "lead time is %uus", sp->p_lead);

  return KATCP_RESULT_OK;
}

int system_source_cmd(struct katcp_dispatch *d, int argc)
{
  struct state_poco *sp;
  char *string;
  int ramp;

  ramp = 0;

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return KATCP_RESULT_FAIL;
  }

  if(argc > 1){
    string = arg_string_katcp(d, 1);
    if(string){
      if(!strcmp(string, "ramp")){
        ramp = 1;
      }
    }
  }

  if(ramp){
    sp->p_source_state &= ~SOURCE_SELECT_VACC_CONTROL0;
  } else {
    sp->p_source_state |= SOURCE_SELECT_VACC_CONTROL0;
  }

  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "using %s data", ramp ? "ramp" : "real");
  return KATCP_RESULT_OK;
}

#define BUFFER 16
int system_info_cmd(struct katcp_dispatch *d, int argc)
{
  struct state_poco *sp;
  struct fixer_poco *fp;
  char input[BUFFER];
  int i;

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return KATCP_RESULT_FAIL;
  }

  check_read_performance(d);

  check_pps_poco(d);
  check_time_poco(d, 1);

#if 0
  do_fix_offset(d);
#endif

  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "%lu.%06lus last sync", sp->p_sync_time.tv_sec, sp->p_sync_time.tv_usec);
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "%uus lead time", sp->p_lead);
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "%s data", (sp->p_source_state & SOURCE_SELECT_VACC_CONTROL0) ? "real" : "ramp");

  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "%s bof image", sp->p_image);
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "%d capture items", sp->p_size);
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "%u desired dsp clock", sp->p_dsp_clock);
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "%u fft window", sp->p_fft_window);
#if 0
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "%u pre accumulation", sp->p_pre_accumulation);
#endif
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "%u accumulation length (%u*%d)", sp->p_accumulation_length, sp->p_dump_count, sp->p_pre_accumulation);
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "0x%x fft shift pattern", sp->p_shift);
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "%u sync period", sp->p_sync_period);
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "%s sync mode", sp->p_manual ? "manual" : "pps");
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "%s sync time updates", sp->p_continuous ? "continuous" : "initial");

#if 0
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "%u local oscillator frequency", sp->p_lo_freq);
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "%lu0fs tau maximum", sp->p_tau_max);
#endif

  dump_timers_katcp(d);

  for(i = 0; i < (POCO_ANT_COUNT * POCO_POLARISATION_COUNT); i++){
    fp = sp->p_fixers[i];
    print_field_poco(input, BUFFER, i, -1);
    input[BUFFER - 1] = '\0';

    log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "input %s coarse 0x%08x", input, fp->f_packed_coarse);
    log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "input %s fine 0x%08x", input, fp->f_packed_fine);
    log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "input %s phase 0x%08x", input, fp->f_packed_phase);
  }

  return KATCP_RESULT_OK;
}
#undef BUFFER

int fft_shift_cmd(struct katcp_dispatch *d, int argc)
{
  struct state_poco *sp;
  unsigned int shift;

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return KATCP_RESULT_FAIL;
  }

  if(argc > 1){
    shift = arg_unsigned_long_katcp(d, 1);
    sp->p_shift = shift;
  }

  load_fft_poco(d);

  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "new shift pattern is 0x%08x", sp->p_shift);

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

  if(sp->p_continuous){
    if(fixup_time_poco(d) < 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "cpu to fpga synchronisation unreliable");
      extra_response_katcp(d, KATCP_RESULT_FAIL, "time");
      return KATCP_RESULT_OWN;
    }
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

    if(cmp_time_katcp(&soonest, &prep) > 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "start time %s already passed or before deadline at %lu.%06lus", ptr, soonest.tv_sec, soonest.tv_usec);
      extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
      return KATCP_RESULT_OWN;
    }

    if(cmp_time_katcp(&prep, &(sp->p_sync_time)) < 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "clock unreasonable as sync time %lu.%06lus is in the future", sp->p_sync_time.tv_sec, sp->p_sync_time.tv_usec);
      extra_response_katcp(d, KATCP_RESULT_FAIL, "reasonability");
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

  if(sp->p_continuous){
    fixup_time_poco(d);
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

#if 0
int poco_delay_cmd(struct katcp_dispatch *d, int argc)
{
  char *input, *ptr;
  struct fixer_poco *fp;
  struct state_poco *sp;
  struct timeval when_tv;
  int delay_cycles, rate_delta, result, ant, from, to, pol, i;
  unsigned int fine_delay, delay_index, delay_offset;
  uint64_t rate_add;
  unsigned int when_extra;
  long delay_time, coarse_delay, rate_time;
  unsigned long adc;
  uint64_t when_cycles, phase;
  uint16_t invert;

  long rate_add_fine;

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
  if((ant < 0) || (ant >= POCO_ANT_COUNT)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "valid antenna values line in the range 0 to %u none of which can be found in parameter %s", POCO_ANT_COUNT - 1, input);
    return KATCP_RESULT_FAIL;
  }
  pol = extract_polarisation_poco(input);
  if(pol < 0){
    from = ant * POCO_POLARISATION_COUNT;
    to = from + POCO_POLARISATION_COUNT;
  } else if(pol >= POCO_POLARISATION_COUNT){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to extract a valid polarisation component from parameter %s", input);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
    return KATCP_RESULT_OWN;
  } else {
    from = (ant * POCO_POLARISATION_COUNT) + pol;
    to = from + 1;
  }
  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "inputs from %d to %d inclusive", from, to - 1);

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
  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "raw mcount when is %llu", when_cycles);

  /* round to the nearest multiple of 256, could use #defines instead of magic constants */
  when_cycles = ((when_cycles & 0x80) ? (when_cycles + 0xff) : when_cycles) & (~0xFFULL);
  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "rounded mcount when is %llu", when_cycles);

  ptr = arg_string_katcp(d, 3); /* delay */
  if(ptr == NULL){
    return KATCP_RESULT_FAIL;
  }
  result = shift_point_string(&delay_time, ptr, 11);
  /* delay time was given in ms, we multiplied it by 10^11 to remove fractions, now in tens of femto-seconds */
  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "string %s converted to %ld", ptr, delay_time);
  if(result < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "delay value %s is too large or otherwise invalid", ptr);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
    return KATCP_RESULT_OWN;
  } 
  if(result > 0){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "discarding delay precision in %s finer than 10^-11", ptr);
  }
  if(delay_time < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "negative delays not reasonable");
    return KATCP_RESULT_FAIL;
  }

  if(sp->p_tau_max < delay_time){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "accepting a delay %lu0fs sheepishly even though it is larger than tau max at %lu0fs", delay_time, sp->p_tau_max);
  }
    
  /* how many adc samples does the delay come to */
  delay_cycles = ((delay_time / 1000) * (adc / 1000000)) / 100000;

  /* remainder is fine delay */
  fine_delay = delay_time % (100000000 / (adc / 1000000));

  coarse_delay = delay_time - fine_delay;

  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "delay of %lu0fs maps to %d samples at %dHz", delay_time, delay_cycles, adc);
  if(delay_cycles > POCO_MAX_COARSE_DELAY){
    log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "requested delay %s exceeds maximum of %dsamples at %luHz", ptr, POCO_MAX_COARSE_DELAY, adc);
    return KATCP_RESULT_FAIL;
  }

  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "coarse delay is %u0fs, fine delay is %u0fs", coarse_delay, fine_delay);
  
  /* this should be wrong - step through the whole table */
  delay_index = (fine_delay * (POCO_FINE_DELAY_TABLE / 16)) / ((100000000 / 16) / (adc / 1000000));
#if 0 /* maybe this instead ? */
  delay_index = (fine_delay * (POCO_FINE_DELAY_TABLE / (2 * 16))) / ((100000000 / 16) / (adc / 1000000));
#endif
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "delay index is %d/%d", delay_index, POCO_FINE_DELAY_TABLE);

  if(delay_index >= (POCO_FINE_DELAY_TABLE / 2)){
#if 1  /* could try this */
    delay_cycles++;
#endif
#if 0 /* could also try this, in combination or without the above option */
    delay_index = POCO_FINE_DELAY_TABLE - delay_index;
#endif
    log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "incrementing coarse delay to %d samples", delay_cycles);
  }

  /* WARNING: unfortunatly need to use 64bits, grr. */
  if(coarse_delay < sp->p_tau_max){
    phase = (sp->p_tau_max - coarse_delay) * (uint64_t)(sp->p_lo_freq);
  } else {
    phase = (coarse_delay - sp->p_tau_max) * (uint64_t)(sp->p_lo_freq);
  }

  /* WARNING: assumes that POCO_FINE_DELAY_TABLE = 2^16: divide delay offset by 5^14 to undo 2^14 tens of femto shift, then multiply by 4 to map a rotation to range 0 - 64k */
  delay_offset = 4 * (phase / (100000000000000ULL / (POCO_FINE_DELAY_TABLE / 4)));

  if(coarse_delay < sp->p_tau_max){ 
    /* WARNING: same assumption, POCO_FINE_DELAY_TABLE = 2^16 */
    invert = (uint16_t)delay_offset; 
    delay_offset = (POCO_FINE_DELAY_TABLE - invert) % POCO_FINE_DELAY_TABLE; 
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
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "rate of %ldfs/s", rate_time);

  if(rate_time == 0){
    rate_delta = 0;
    rate_add = 0;
    rate_add_fine = 0;
  } else {
    /* currently only adjust by single steps */

    rate_delta = (rate_time < 0) ? (-1) : 1;
    /* make rate_add positive no matter what sign_rate time */
    /*rate_add = rate_delta * 1000000000 / (rate_time * (adc / USECPERSEC_POCO));*/

    rate_add_fine = rate_time * ((adc / USECPERSEC_POCO) * (adc / USECPERSEC_POCO) / 1000) / (256 * sp->p_fft_window); 
    
    /*rate_add = ((adc  / (rate_add_fine * 256))*1000)/512;i*/
    rate_add = ((uint64_t) sp->p_dsp_clock) * (adc/256)/(rate_add_fine*512);

    rate_add_fine *= -1;

    if(rate_add <= 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "update rate too fast (%ld * %lu)", rate_time, adc / USECPERSEC_POCO);
      return -1;
    }
  }

  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "need to update coarse by %d every %llu milliseconds", rate_delta, rate_add);
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "need to update fine by %ld", rate_add_fine);
  
  for(i = from; i < to; i++){
    fp = sp->p_fixers[i];

    fp->f_stage_mcount = when_cycles;
    fp->f_stage_delay = delay_cycles;

    fp->f_stage_add = rate_add;
    fp->f_stage_delta = rate_delta;

    fp->f_stage_fdrate = rate_add_fine;

    fp->f_stage_slope = delay_index;
    fp->f_stage_offset = delay_offset;

#if 0
    log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "scheduling fixer[%d] for %lu.%06lus", i, headstart.tv_sec, headstart.tv_usec);
#endif

    schedule_fixer(d, fp);
  }
   
  return KATCP_RESULT_OK;
}
#endif

int poco_delay_cmd(struct katcp_dispatch *d, int argc)
{
  char *input, *ptr;
  struct fixer_poco *fp;
  struct state_poco *sp;
  struct timeval when_tv, now_tv, delta_tv;

  int result, ant, from, to, pol, i;
  unsigned int when_extra;

  unsigned long adc;
  uint64_t when_cycles;

  uint32_t fringe_packed_value;
  uint32_t fine_packed_value;
  uint32_t coarse_packed_value;

  unsigned int coarse_delay, rounded_fine_delay;
  int fine_delay_rate, rounded_fringe_phase, rounded_fringe_rate;

  double fine_delay;
  double fringe_phase, fringe_rate, delay_value, delay_rate; /* TODO:  */
  double actual_fringe_phase, actual_fringe_rate, actual_delay_value, actual_delay_rate; 

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return KATCP_RESULT_FAIL;
  }

#ifdef DEBUG
  fp = NULL;
#endif

  adc = sp->p_dsp_clock * POCO_ADC_DSP_FACTOR;

  if(argc < 4){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "need channel, time, delay and optional delay-rate, fringe and fringe-rate as parameters");
    extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
    return KATCP_RESULT_OWN;
  }

  input = arg_string_katcp(d, 1); /* input */
  if(input == NULL){
    return KATCP_RESULT_FAIL;
  }
  ant = extract_ant_poco(input);
  if((ant < 0) || (ant >= POCO_ANT_COUNT)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "valid antenna values line in the range 0 to %u none of which can be found in parameter %s", POCO_ANT_COUNT - 1, input);
    return KATCP_RESULT_FAIL;
  }
  pol = extract_polarisation_poco(input);
  if(pol < 0){
    from = ant * POCO_POLARISATION_COUNT;
    to = from + POCO_POLARISATION_COUNT;
  } else if(pol >= POCO_POLARISATION_COUNT){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to extract a valid polarisation component from parameter %s", input);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
    return KATCP_RESULT_OWN;
  } else {
    from = (ant * POCO_POLARISATION_COUNT) + pol;
    to = from + 1;
  }
  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "inputs from %d to %d inclusive", from, to - 1);

  ptr = arg_string_katcp(d, 2); /* time */
  if(ptr == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "require a time parameter");
    return KATCP_RESULT_FAIL;
  }

  if(time_from_string(&when_tv, &when_extra, ptr) < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "%s not a well-formed time", ptr);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
    return KATCP_RESULT_OWN;
  }

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "%s maps to %lu.%06lus + %lufs", ptr, when_tv.tv_sec, when_tv.tv_usec, when_extra);

  delta_tv.tv_sec = 0;
  delta_tv.tv_usec = sp->p_lead;

  gettimeofday(&now_tv, NULL);

  if((when_tv.tv_sec == 0) && (when_tv.tv_usec == 0) && (when_extra == 0)){
    add_time_katcp(&when_tv, &now_tv, &delta_tv);

    when_tv.tv_sec++;
    when_extra = 0;
  } else {
    add_time_katcp(&now_tv, &now_tv, &delta_tv);

    if(cmp_time_katcp(&when_tv, &now_tv) <= 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "requested time too soon given lead time of %uus", sp->p_lead);
      return KATCP_RESULT_FAIL;
    }
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
  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "raw mcount when is %llu", when_cycles);

  /* round to the nearest multiple of 256, could use #defines instead of magic constants */
  when_cycles = ((when_cycles & 0x80) ? (when_cycles + 0xff) : when_cycles) & (~0xFFULL);
  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "rounded mcount when is %llu", when_cycles);

  ptr = arg_string_katcp(d, 3); /* delay */
  if(ptr == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "need to specify a delay in milliseconds");
    return KATCP_RESULT_FAIL;
  }
  delay_value = atof(ptr) / 1000.0;

#if 0
  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "parsing delay string <%s> into %f", ptr, delay_value);
#endif

  if(argc > 4){
    ptr = arg_string_katcp(d, 4); /* delay rate */
    if(ptr == NULL){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to acquire delay rate");
      return KATCP_RESULT_FAIL;
    }
    delay_rate = atof(ptr);
  } else {
    delay_rate = 0.0;
  }

  if(argc > 5){
    ptr = arg_string_katcp(d, 5); /* fringe offset */
    if(ptr == NULL){
      return KATCP_RESULT_FAIL;
    }
    fringe_phase = atof(ptr);
  } else {
    fringe_phase = 0.0;
  }

  if(argc > 6){
    ptr = arg_string_katcp(d, 6); /* fringe rate */
    if(ptr == NULL){
      return KATCP_RESULT_FAIL;
    }
    fringe_rate = atof(ptr);
  } else {
    fringe_rate = 0.0;
  }

  coarse_delay = (unsigned)(delay_value * adc);
  fine_delay = (delay_value * adc) - coarse_delay;
  rounded_fine_delay = (unsigned)(fine_delay * ((uint64_t)1 << (FINE_DELAY_BITS)));
  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "requested delay %gs, loading coarse delay %u, fine delay %f", delay_value, coarse_delay, fine_delay);

  fine_delay_rate  = (signed int)(delay_rate * ((uint64_t)1 << (FIXER_SCHEDULE + FINE_DELAY_RATE_BITS)));
  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "requested delay rate %gms/ms, loading value %d", delay_rate, fine_delay_rate);

  rounded_fringe_phase = (signed int)((fringe_phase / PI_FLOAT_POCO) * ((uint64_t)1 << (FRINGE_OFFSET_BITS - 1)));
  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "requested fringe phase %g, loaded value %d", fringe_phase, rounded_fringe_phase);

  rounded_fringe_rate = (signed int)((fringe_rate / (sp->p_dsp_clock * PI_FLOAT_POCO * 2.0 * 0.001)) * ((uint64_t)1 << (FIXER_SCHEDULE + FRINGE_RATE_BITS - 1)));
  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "requested fringe rate %gHz, loading value %d", fringe_rate, rounded_fringe_rate);

#if 0
  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "delay value is %f, factor %u, result is %u", delay_value, adc, coarse_delay);
#endif

  if((fringe_phase != 0) && (rounded_fringe_phase == 0)){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "fringe phase too small for available resolution");
  }
  if((fringe_rate != 0) && (rounded_fringe_rate == 0)){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "fringe rate too slow for available resolution");
  }

  if(delay_rate != 0.0){
    if(fine_delay_rate == 0){
      log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "desired delay rate %g too slow", delay_rate);
    }
    if(abs(fine_delay_rate) > (1 << (FINE_DELAY_RATE_BITS))){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "delay rate exceeds maximum");
      return KATCP_RESULT_FAIL;
    }
  }

  if(delay_value != 0.0){
    if((rounded_fine_delay == 0) && (coarse_delay == 0)){
      log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "requested delay is too small");
    }
    if(abs(rounded_fine_delay) > (1 << (FINE_DELAY_BITS))){
      log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "internal logic error, modulo logic invalid");
      return KATCP_RESULT_FAIL;
    }
    if(coarse_delay > (1 << COARSE_DELAY_BITS)){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "desired delay %g is too large (coarse: value=%u, limit=%u)", delay_value, coarse_delay, (1 << COARSE_DELAY_BITS));
      return KATCP_RESULT_FAIL;
    }
  }

  /* schedule logic to wake at overflow time, cancel it when new values are loaded */
  /* check for less than or equal to zero if rate is positive (logic is inverted) */

  actual_delay_value = ((double)coarse_delay + ((double)(rounded_fine_delay) / (((uint64_t)1 << (FINE_DELAY_BITS))))) / adc;
  actual_delay_rate  = ((double)(fine_delay_rate)) / ((uint64_t)1 << (FIXER_SCHEDULE + FRINGE_RATE_BITS));

  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "actual delay is %gms updated by %gms/ms\n", actual_delay_value * 1000.0, actual_delay_rate);

  actual_fringe_phase = PI_FLOAT_POCO * ((double)rounded_fringe_phase / (((uint64_t)1 << (FRINGE_OFFSET_BITS - 1))));
  actual_fringe_rate = PI_FLOAT_POCO * 2.0 * 0.001 * (double)(sp->p_dsp_clock) * ((double)rounded_fringe_rate / ((uint64_t)1 << (FRINGE_RATE_BITS + FIXER_SCHEDULE - 1)));

  if((fringe_phase != 0.0) || (fringe_rate != 0)){
    log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "actual fringe is %grad updated at %grad/ms", actual_fringe_phase, actual_fringe_rate);
  }

  coarse_packed_value = coarse_delay;
  fine_packed_value   = (    (fine_delay_rate << 16) & 0xffff0000) | (rounded_fine_delay    & 0xffff);
  fringe_packed_value = ((rounded_fringe_rate << 16) & 0xffff0000) | (rounded_fringe_phase  & 0xffff);

  for(i = from; i < to; i++){
    fp = sp->p_fixers[i];

    fp->f_mcount = when_cycles;

    fp->f_packed_coarse = coarse_packed_value;
    fp->f_packed_fine = fine_packed_value;
    fp->f_packed_phase = fringe_packed_value;

    schedule_fixer(d, fp);
  }
   
  return KATCP_RESULT_OK;
}


#if 0
  long rate_add_fine;

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
  if((ant < 0) || (ant >= POCO_ANT_COUNT)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "valid antenna values line in the range 0 to %u none of which can be found in parameter %s", POCO_ANT_COUNT - 1, input);
    return KATCP_RESULT_FAIL;
  }
  pol = extract_polarisation_poco(input);
  if(pol < 0){
    from = ant * POCO_POLARISATION_COUNT;
    to = from + POCO_POLARISATION_COUNT;
  } else if(pol >= POCO_POLARISATION_COUNT){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to extract a valid polarisation component from parameter %s", input);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
    return KATCP_RESULT_OWN;
  } else {
    from = (ant * POCO_POLARISATION_COUNT) + pol;
    to = from + 1;
  }
  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "inputs from %d to %d inclusive", from, to - 1);

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
  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "raw mcount when is %llu", when_cycles);

  /* round to the nearest multiple of 256, could use #defines instead of magic constants */
  when_cycles = ((when_cycles & 0x80) ? (when_cycles + 0xff) : when_cycles) & (~0xFFULL);
  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "rounded mcount when is %llu", when_cycles);

  ptr = arg_string_katcp(d, 3); /* delay */
  if(ptr == NULL){
    return KATCP_RESULT_FAIL;
  }
  result = shift_point_string(&delay_time, ptr, 11);
  /* delay time was given in ms, we multiplied it by 10^11 to remove fractions, now in tens of femto-seconds */
  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "string %s converted to %ld", ptr, delay_time);
  if(result < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "delay value %s is too large or otherwise invalid", ptr);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
    return KATCP_RESULT_OWN;
  } 
  if(result > 0){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "discarding delay precision in %s finer than 10^-11", ptr);
  }
  if(delay_time < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "negative delays not reasonable");
    return KATCP_RESULT_FAIL;
  }

  if(sp->p_tau_max < delay_time){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "accepting a delay %lu0fs sheepishly even though it is larger than tau max at %lu0fs", delay_time, sp->p_tau_max);
  }
    
  /* how many adc samples does the delay come to */
  delay_cycles = ((delay_time / 1000) * (adc / 1000000)) / 100000;

  /* remainder is fine delay */
  fine_delay = delay_time % (100000000 / (adc / 1000000));

  coarse_delay = delay_time - fine_delay;

  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "delay of %lu0fs maps to %d samples at %dHz", delay_time, delay_cycles, adc);
  if(delay_cycles > POCO_MAX_COARSE_DELAY){
    log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "requested delay %s exceeds maximum of %dsamples at %luHz", ptr, POCO_MAX_COARSE_DELAY, adc);
    return KATCP_RESULT_FAIL;
  }

  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "coarse delay is %u0fs, fine delay is %u0fs", coarse_delay, fine_delay);
  
  /* this should be wrong - step through the whole table */
  delay_index = (fine_delay * (POCO_FINE_DELAY_TABLE / 16)) / ((100000000 / 16) / (adc / 1000000));
#if 0 /* maybe this instead ? */
  delay_index = (fine_delay * (POCO_FINE_DELAY_TABLE / (2 * 16))) / ((100000000 / 16) / (adc / 1000000));
#endif
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "delay index is %d/%d", delay_index, POCO_FINE_DELAY_TABLE);

  if(delay_index >= (POCO_FINE_DELAY_TABLE / 2)){
#if 1  /* could try this */
    delay_cycles++;
#endif
#if 0 /* could also try this, in combination or without the above option */
    delay_index = POCO_FINE_DELAY_TABLE - delay_index;
#endif
    log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "incrementing coarse delay to %d samples", delay_cycles);
  }

  /* WARNING: unfortunatly need to use 64bits, grr. */
  if(coarse_delay < sp->p_tau_max){
    phase = (sp->p_tau_max - coarse_delay) * (uint64_t)(sp->p_lo_freq);
  } else {
    phase = (coarse_delay - sp->p_tau_max) * (uint64_t)(sp->p_lo_freq);
  }

  /* WARNING: assumes that POCO_FINE_DELAY_TABLE = 2^16: divide delay offset by 5^14 to undo 2^14 tens of femto shift, then multiply by 4 to map a rotation to range 0 - 64k */
  delay_offset = 4 * (phase / (100000000000000ULL / (POCO_FINE_DELAY_TABLE / 4)));

  if(coarse_delay < sp->p_tau_max){ 
    /* WARNING: same assumption, POCO_FINE_DELAY_TABLE = 2^16 */
    invert = (uint16_t)delay_offset; 
    delay_offset = (POCO_FINE_DELAY_TABLE - invert) % POCO_FINE_DELAY_TABLE; 
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
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "rate of %ldfs/s", rate_time);

  if(rate_time == 0){
    rate_delta = 0;
    rate_add = 0;
    rate_add_fine = 0;
  } else {
    /* currently only adjust by single steps */

    rate_delta = (rate_time < 0) ? (-1) : 1;
    /* make rate_add positive no matter what sign_rate time */
    /*rate_add = rate_delta * 1000000000 / (rate_time * (adc / USECPERSEC_POCO));*/

    rate_add_fine = rate_time * ((adc / USECPERSEC_POCO) * (adc / USECPERSEC_POCO) / 1000) / (256 * sp->p_fft_window); 
    
    /*rate_add = ((adc  / (rate_add_fine * 256))*1000)/512;i*/
    rate_add = ((uint64_t) sp->p_dsp_clock) * (adc/256)/(rate_add_fine*512);

    rate_add_fine *= -1;

    if(rate_add <= 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "update rate too fast (%ld * %lu)", rate_time, adc / USECPERSEC_POCO);
      return -1;
    }
  }

  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "need to update coarse by %d every %llu milliseconds", rate_delta, rate_add);
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "need to update fine by %ld", rate_add_fine);
  
  for(i = from; i < to; i++){
    fp = sp->p_fixers[i];

    fp->f_stage_mcount = when_cycles;
    fp->f_stage_delay = delay_cycles;

    fp->f_stage_add = rate_add;
    fp->f_stage_delta = rate_delta;

    fp->f_stage_fdrate = rate_add_fine;

    fp->f_stage_slope = delay_index;
    fp->f_stage_offset = delay_offset;

#if 0
    log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "scheduling fixer[%d] for %lu.%06lus", i, headstart.tv_sec, headstart.tv_usec);
#endif

    schedule_fixer(d, fp);
  }
   
  return KATCP_RESULT_OK;
}
#endif

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
  char *input;
  char *phase_string;
  struct fixer_poco *fp;
  struct state_poco *sp;
  int result, ant, from, to, pol, i;
  long phase;
  unsigned int raw_offset;
  
  sp = need_current_mode_katcp(d, POCO_POCO_MODE);

  if(argc < 3){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "need input and phase");
    extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
    return KATCP_RESULT_OWN;
  }

  input = arg_string_katcp(d, 1); /* input */
  if(input == NULL){
    return KATCP_RESULT_FAIL;
  }
  ant = extract_ant_poco(input);
  if((ant < 0) || (ant >= POCO_ANT_COUNT)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "valid antenna values line in the range 0 to %u none of which can be found in parameter %s", POCO_ANT_COUNT - 1, input);
    return KATCP_RESULT_FAIL;
  }
  pol = extract_polarisation_poco(input);
  if(pol < 0){
    from = ant * POCO_POLARISATION_COUNT;
    to = from + POCO_POLARISATION_COUNT;
  } else if(pol >= POCO_POLARISATION_COUNT){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to extract a valid polarisation component from parameter %s", input);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
    return KATCP_RESULT_OWN;
  } else {
    from = (ant * POCO_POLARISATION_COUNT) + pol;
    to = from + 1;
  }
  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "inputs from %d to %d inclusive", from, to - 1);

  if(argc > 2){
    phase_string = arg_string_katcp(d, 2); /* phase constant */
    if(phase_string == NULL){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to acquire phase parameter");
      return KATCP_RESULT_FAIL;
    }
    result = shift_point_string(&phase, phase_string, 6);
    if(result < 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "phase value is too large or otherwise invalid");
      extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
      return KATCP_RESULT_OWN;
    } 
    if(result > 0){
      log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "discarding excess phase precision");
    }

    if((phase > PI_MICRO_POCO) || (phase < -PI_MICRO_POCO)){
      log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "phase value %s not within half a rotation of zero", phase_string);
      /*phase %= PI_MICRO_POCO;
      phase *= -1;*/
    }
    
    log_message_katcp(d,KATCP_LEVEL_INFO,NULL,"phase value before conversion %d",phase);

/*==========================*/
    
    if ((phase > 2*PI_MICRO_POCO) || (phase < 2*PI_MICRO_POCO)){
        phase %= 2*PI_MICRO_POCO;
    }

    if (phase >= 0){
      raw_offset = phase / (PI_MICRO_POCO / POCO_FINE_DELAY_TABLE * 2);        
    } else {
      raw_offset = (phase + 2 * PI_MICRO_POCO) / (PI_MICRO_POCO / POCO_FINE_DELAY_TABLE * 2);
#if 0
      /* err, shouldn't it be rather */
      raw_offset = ((2 * PI_MICRO_POCO) - phase) / (PI_MICRO_POCO / POCO_FINE_DELAY_TABLE * 2);
#endif
    }

    for(i = from; i < to; i++){
      fp = sp->p_fixers[i];

      fp->f_base_offset = raw_offset;
      log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "setting base_offset to %d/%d", fp->f_base_offset, POCO_FINE_DELAY_TABLE);
     /* log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "setting fixer %d from %d to %ldmr", i, fp->f_phase_ur, phase);

      fp->f_phase_ur = phase;*/
    }
  }

  /*prepend_reply_katcp(d);
  append_string_katcp(d, KATCP_FLAG_STRING, KATCP_OK);

  i = from;

  while(i < to){
    fp = sp->p_fixers[i];
    i++;
*/
    /* WARNING: mod operator can yield negative results */
    /*append_args_katcp(d, (i == to) ? KATCP_FLAG_LAST : 0, "%d.%lu", fp->f_phase_ur / 1000000, (unsigned int) (fp->f_phase_ur) % 1000000);
     */
 /* }*/

  return KATCP_RESULT_OK;

  /*log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "phase no longer used");
  return KATCP_RESULT_FAIL;
  
  return KATCP_RESULT_OK;*/

#if 0
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

    result = shift_point_string(&phase, phase_string, 6);
    if(result < 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "phase value is too large or otherwise invalid");
      extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
      return KATCP_RESULT_OWN;
    } 
    if(result > 0){
      log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "discarding excess phase precision");
    }

    if((phase > PI_MICRO_POCO) || (phase < (-PI_MICRO_POCO))){
      log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "phase value %s not within half a rotation of zero", phase_string);
    }

    for(i = from; i < to; i++){
      fp = sp->p_fixers[i];

      log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "setting fixer %d from %d to %ldmr", i, fp->f_phase_ur, phase);

      fp->f_phase_ur = phase;
    }
  }

  prepend_reply_katcp(d);
  append_string_katcp(d, KATCP_FLAG_STRING, KATCP_OK);

  i = from;

  while(i < to){
    fp = sp->p_fixers[i];
    i++;

    /* WARNING: mod operator can yield negative results */
    append_args_katcp(d, (i == to) ? KATCP_FLAG_LAST : 0, "%d.%lu", fp->f_phase_ur / 1000000, (unsigned int) (fp->f_phase_ur) % 1000000);
  }

  return KATCP_RESULT_OWN;
#if 0
  return KATCP_RESULT_OK;
#endif
#endif
}


int poco_lo_freq_cmd(struct katcp_dispatch *d, int argc)
{
  struct state_poco *sp;
  unsigned long lo;

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return KATCP_RESULT_FAIL;
  }

  if(argc >= 2){
    lo = arg_unsigned_long_katcp(d, 1);
    if(lo != 0){
      if(lo < POCO_LO_FREQ_MIN){
        log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "local osciallator frequency too low or otherwise malformed");
        extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
        return KATCP_RESULT_OWN;
      }
      if(lo > POCO_LO_FREQ_MAX){
        log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "local osciallator frequency too high");
        extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
        return KATCP_RESULT_OWN;
      } 
    }
    sp->p_lo_freq = lo;
  }

  prepend_reply_katcp(d);
  append_string_katcp(d, KATCP_FLAG_STRING, KATCP_OK);
  append_unsigned_long_katcp(d, KATCP_FLAG_LAST | KATCP_FLAG_ULONG , (unsigned long) sp->p_lo_freq);

  return KATCP_RESULT_OWN;
}

int poco_tau_max_cmd(struct katcp_dispatch *d, int argc)
{
  struct state_poco *sp;
  char *ptr;
  long tau_max, biggest_delay;
  int result;
  unsigned long adc;

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  adc = sp->p_dsp_clock * POCO_ADC_DSP_FACTOR;
  
  if (sp == NULL){
    return KATCP_RESULT_FAIL;
  }

  if (argc >= 2){
    ptr = arg_string_katcp(d,1);
    
    if(ptr == NULL){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to acquire tau max value");
      return KATCP_RESULT_FAIL;
    } 
     
    result = shift_point_string(&tau_max, ptr, 11);
   
    if(result < 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "tau max value %s is too large or otherwise invalid", ptr);
      extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
      return KATCP_RESULT_OWN;
    } 
    if(result > 0){
      log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "discarding excess tau max precision in %s", ptr);
    }

    biggest_delay = (1000000000 / (adc / 100000)) * POCO_MAX_COARSE_DELAY;

    if(tau_max > (biggest_delay / 2)){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "tau max of %ld0fs larger than limit of %ld0fs", tau_max, biggest_delay / 2);
      extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
      return KATCP_RESULT_OWN;
    }

    log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "set tau max to %ld0fs", tau_max);

    sp->p_tau_max = tau_max; 
  }

  prepend_inform_katcp(d);
  append_string_katcp(d, KATCP_FLAG_STRING, KATCP_OK);
  append_unsigned_long_katcp(d, KATCP_FLAG_LAST | KATCP_FLAG_ULONG , (unsigned long) sp->p_tau_max);

  return KATCP_RESULT_OWN;
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

  sp->p_tau_max = 0;
  sp->p_lo_freq = 0;

  /* mcount unset */

  sp->p_manual = POCO_DEFAULT_SYNC_MODE; /* set in config.h, normally not manual */
  sp->p_continuous = POCO_DEFAULT_UPDATE_BEHAVIOUR;
  sp->p_shift = POCO_DEFAULT_FFT_SHIFT;

  /* gateware overflow check */
  sp->p_overflow_sensor.a_status = NULL;
  sp->p_overflow_sensor.a_control = NULL;
  
#if 0
  /* psu/system stuff */
  for(i = 0; i < HWMON_COUNT_POCO; i++){
    sp->p_hwmon_sensor[i].h_fd = (-1);
  }
#endif

  /* clock sync */
  sp->p_ntp_sensor.n_fd = (-1); 

#if 0
  sp->p_bs_gateware = (-1);
#endif

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

  if(mode_version_katcp(d, POCO_POCO_MODE, NULL, 0, 1)){
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
  a = setup_boolean_acquire_katcp(d, &acquire_ntp_poco, &(sp->p_ntp_sensor));
  if(a == NULL){
    fprintf(stderr, "setup: unable to allocate ntp acquisition\n");
    return -1;
  }
  if(register_direct_multi_boolean_sensor_katcp(d, POCO_POCO_MODE, "poco.timing.sync", "clock good", "none", a) < 0){
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
  result += register_mode_katcp(d, "?poco-delay", "sets the coarse delay (?poco-delay antenna[polarisation] time delay delay-rate fringe-offset fringe-rate)", &poco_delay_cmd, POCO_POCO_MODE);

  result += register_mode_katcp(d, "?poco-snap-shot", "grabs a snap shot", &poco_snap_shot_cmd, POCO_POCO_MODE);

  /* not yet available */
  result += register_mode_katcp(d, "?poco-phase", "sets the fine delay (unimplemented)", &poco_phase_cmd, POCO_POCO_MODE);
  result += register_mode_katcp(d, "?poco-lo-freq", "sets the local oscillator frequency (unimplemented)", &poco_lo_freq_cmd, POCO_POCO_MODE);
  result += register_mode_katcp(d, "?poco-tau-max", "sets the total delay (unimplemented)", &poco_tau_max_cmd, POCO_POCO_MODE);

  /* commands outside ICD */
  result += register_mode_katcp(d, "?system-source", "selects between ramp and real data (?system-source [ramp|real])", &system_source_cmd, POCO_POCO_MODE);
  result += register_mode_katcp(d, "?system-info", "display various pieces of system information (?system-info)", &system_info_cmd, POCO_POCO_MODE);
  result += register_mode_katcp(d, "?system-lead", "set the lead time (?system-info lead-time)", &system_lead_cmd, POCO_POCO_MODE);

  result += register_mode_katcp(d, "?option-set", "sets options (?option-set [option])", &option_set_cmd, POCO_POCO_MODE);
  result += register_mode_katcp(d, "?sync-now", "resync the system (?sync-now)", &sync_now_cmd, POCO_POCO_MODE);

  result += register_mode_katcp(d, "?fft-shift", "set the shift register (?fft-shift [value])", &fft_shift_cmd, POCO_POCO_MODE);

#ifdef HACK
  result += register_mode_katcp(d, "?wordread", "reads a word (?wordread register-name word-offset length)", &word_read_cmd, POCO_POCO_MODE);
  result += register_mode_katcp(d, "?wordwrite", "writes a word (?wordwrite register-name word-offset payload ...)", &word_write_cmd, POCO_POCO_MODE);
#endif

#if 0
  result += register_mode_katcp(d, "?capture-poll", "set the poll interval for a capture stream (?capture-poll name [period])", &capture_poll_cmd, POCO_POCO_MODE);
#endif

  if(result < 0){
    fprintf(stderr, "setup: unable to register poco mode commands\n");
    return -1;
  }

  return 0;
}
