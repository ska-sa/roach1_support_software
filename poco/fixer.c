#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "katcp.h"
#include "katpriv.h"

#include "core.h"
#include "modes.h"
#include "poco.h"
#include "registers.h"

#define NAME_BUFFER 64

#define FIXER_MAGIC 0xf178e72

struct fixer_poco *startup_fixer(struct katcp_dispatch *d, int instance)
{
  struct fixer_poco *fp;
  struct state_poco *sp;

  sp = get_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return NULL;
  }

  fp = malloc(sizeof(struct fixer_poco));
  if(fp == NULL){
    return NULL;
  }
 
  fp->f_magic = FIXER_MAGIC;
  fp->f_id = instance;
  fp->f_up = 0;
  fp->f_fresh = 0;

  fp->f_this_mcount = 0;
  fp->f_this_add = 0;

  fp->f_this_delta = 0;
  fp->f_this_delay = 0;

  fp->f_stage_mcount = 0;
  fp->f_stage_add = 0;

  fp->f_stage_delta = 0;
  fp->f_stage_delay = 0;

  fp->f_phase_mr = 0;

  fp->f_status = NULL;
  fp->f_control = NULL;
  fp->f_next_value = NULL;
  fp->f_next_msb = NULL;
  fp->f_next_lsb = NULL;

  return fp;
}

void shutdown_fixer(struct katcp_dispatch *d, struct fixer_poco *fp)
{
  struct state_poco *sp;

#ifdef DEBUG
  if(fp->f_magic != FIXER_MAGIC){
    fprintf(stderr, "fixer: shutdown problem: bad magic\n");
    abort();
  }
#endif

  sp = get_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return;
  }

  if(fp->f_up){
    discharge_timer_katcp(d, fp);
    fp->f_up = 0;
  }

  free(fp);
}

int run_fixer(struct katcp_dispatch *d, void *data)
{
  struct fixer_poco *fp;
  struct state_poco *sp;
  struct timeval actual, delta, headstart;
  uint32_t value;
  int rr, wr;

  fp = data;

#ifdef DEBUG
  if(fp->f_magic != FIXER_MAGIC){
    fprintf(stderr, "fixer: logic problem: bad magic\n");
    abort();
  }

  if(fp->f_up == 0){
    fprintf(stderr, "fixer: not marked as up but still running\n");
    abort();
  }
#endif

  fp->f_up = 0; /* assume we won't run again */

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "disabling fixer, poco mode not available");
    return -1;
  }

  if(fp->f_this_delay < 0){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "coarse delay reached minimum at zero");
    fp->f_this_delay = 0;
    fp->f_this_delta = 0;
  } else if(fp->f_this_delay >= POCO_MAX_COARSE_DELAY){
    fp->f_this_delay = POCO_MAX_COARSE_DELAY - 1;
    fp->f_this_delta = 0;
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "coarse delay reached maximum of %d samples", fp->f_this_delay);
  } 

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "fixer %d should load delay of %u", fp->f_id, fp->f_this_delay);

  rr = read_pce(d, fp->f_status, &value, 0, 4);
  if(rr != 4){
    return -1;
  }

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "status is 0x%08x", value);

  if(value & IS_COARSE_ARMED_STATUS((0x1 << fp->f_id))){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "fixer %d already armed in status 0x%08x", fp->f_id, value);
  }

  value = fp->f_this_delay;
  wr = write_pce(d, fp->f_next_value, &value, 0, 4);
  if(wr != 4){
    return -1;
  }

  value = (fp->f_this_mcount >> 32) & 0xffffffff;
  wr = write_pce(d, fp->f_next_msb, &value, 0, 4);
  if(wr != 4){
    return -1;
  }

  value = fp->f_this_mcount & 0xffffffff;
  wr = write_pce(d, fp->f_next_lsb, &value, 0, 4);
  if(wr != 4){
    return -1;
  }

  value = 0;
  wr = write_pce(d, fp->f_control, &value, 0, 4);
  if(wr != 4){
    return -1;
  }

  value = COARSE_DELAY_ARM_CONTROL((0x1 << fp->f_id)) | CLEAR_MISS_CONTROL((0x1 << fp->f_id));
  wr = write_pce(d, fp->f_control, &value, 0, 4);
  if(wr != 4){
    return -1;
  }

  rr = read_pce(d, fp->f_status, &value, 0, 4);
  if(rr != 4){
    return -1;
  }
  if(!(value & IS_COARSE_ARMED_STATUS((0x1 << fp->f_id)))){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "unable to arm delay logic");
  }
  if((value & MISSED_DELAY_STATUS((0x1 << fp->f_id)))){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "coarse delay loaded too late");
  }

  /* tick over */
  fp->f_this_delay += fp->f_this_delta;
  fp->f_this_mcount += (uint64_t)sp->p_dsp_clock * fp->f_this_add;
  
  /* cut over to new values for next schedule */
  if(fp->f_fresh && (fp->f_this_mcount >= fp->f_stage_mcount)){
    log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "intending to cut over to new rate");

    fp->f_this_mcount = fp->f_stage_mcount;
    fp->f_this_add = fp->f_stage_add;

    fp->f_this_delay = fp->f_stage_delay;
    fp->f_this_delta = fp->f_stage_delta;

    fp->f_fresh = 0;
  }

  if(fp->f_this_delta){

    mcount_to_tv_poco(d, &actual, &(fp->f_this_mcount));

    delta.tv_sec = 0;
    delta.tv_usec = sp->p_lead;

    sub_time_katcp(&headstart, &actual, &delta);

    if(register_at_tv_katcp(d, &headstart, &run_fixer, fp) < 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to reregister timer");
      return -1;
    }

    fp->f_up = 1;
  }

  return 0;
}

int schedule_fixer(struct katcp_dispatch *d, struct fixer_poco *fp)
{
  char buffer[NAME_BUFFER];
  struct timeval actual, headstart, delta;
  struct state_poco *sp;

  sp = get_mode_katcp(d, POCO_POCO_MODE);
#ifdef DEBUG
  if(sp == NULL){
    fprintf(stderr, "fixer: logic problem: poco mode not available\n");
    abort();
  }
  if(fp->f_magic != FIXER_MAGIC){
    fprintf(stderr, "fixer: logic problem: bad magic while scheduling\n");
    abort();
  }
#endif

  snprintf(buffer, NAME_BUFFER - 1, CD_MSB_SKEL_POCO_REGISTER, fp->f_id);
  buffer[NAME_BUFFER - 1] = '\0';
  fp->f_next_msb = by_name_pce(d, buffer);

  snprintf(buffer, NAME_BUFFER - 1, CD_LSB_SKEL_POCO_REGISTER, fp->f_id);
  buffer[NAME_BUFFER - 1] = '\0';
  fp->f_next_lsb = by_name_pce(d, buffer);

  snprintf(buffer, NAME_BUFFER - 1, CD_VAL_SKEL_POCO_REGISTER, fp->f_id);
  buffer[NAME_BUFFER - 1] = '\0';
  fp->f_next_value = by_name_pce(d, buffer);

  fp->f_status = by_name_pce(d, STATUS_POCO_REGISTER);
  fp->f_control = by_name_pce(d, CONTROL_POCO_REGISTER);

  if((fp->f_next_value == NULL) || 
     (fp->f_next_lsb == NULL) || 
     (fp->f_next_msb == NULL) || 
     (fp->f_status == NULL) || 
     (fp->f_control == NULL)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to locate registers needed for coarse delay");
    return -1;
  }

  if((fp->f_up == 0) || (fp->f_this_mcount > fp->f_stage_mcount)){
    /* not running, or would run later than desired */
#ifdef DEBUG
    if(fp->f_stage_mcount == 0){
      fprintf(stderr, "fixer: major logic problem: next mcount not set\n");
      abort();
    }
#endif

    fp->f_this_mcount = fp->f_stage_mcount;
    fp->f_this_delay = fp->f_stage_delay;

    fp->f_this_add = fp->f_stage_add;
    fp->f_this_delta = fp->f_stage_delta;

    mcount_to_tv_poco(d, &actual, &(fp->f_stage_mcount));

    delta.tv_sec = 0;
    delta.tv_usec = sp->p_lead;

    sub_time_katcp(&headstart, &actual, &delta);

    if(register_at_tv_katcp(d, &headstart, &run_fixer, fp) < 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to register timer");
      return -1;
    }

    fp->f_up = 1;
  } else {
    /* the next_* should really be parameters to schedule, then the fresh flag is safer */
    fp->f_fresh = 1;
  }

  return 0;
}
