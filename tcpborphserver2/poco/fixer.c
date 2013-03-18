#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "katcp.h"
#include "katpriv.h"

#include "core.h"
#include "modes.h"
#include "poco.h"
#include "registers.h"
#include "misc.h"

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

  fp->f_stage_fdrate = 0;
  fp->f_this_fdrate = 0;

  fp->f_stage_mcount = 0;
  fp->f_stage_add = 0;

  fp->f_stage_delta = 0;
  fp->f_stage_delay = 0;

  fp->f_stage_slope = 0;
  fp->f_stage_offset = 0;

  fp->f_base_offset = 0;

#if 0
  fp->f_control = NULL;
  fp->f_control0 = NULL;

  fp->f_status = NULL;
  fp->f_status0 = NULL;
#endif

  fp->f_mcount = 0;

  fp->f_packed_coarse = 0;
  fp->f_packed_fine = 0;
  fp->f_packed_phase = 0;

  fp->f_register_time_msw = NULL;
  fp->f_register_time_lsw = NULL;
  fp->f_register_time_status = NULL;

  fp->f_register_coarse = NULL;
  fp->f_register_fine = NULL;
  fp->f_register_phase = NULL;

  return fp;
}

void shutdown_fixer(struct katcp_dispatch *d, struct fixer_poco *fp)
{
  struct state_poco *sp;

  if(fp == NULL){
    return;
  }

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

  /* This should all be updated in multiples of 256, 512/2 (fft/2, where 2 is the amount by which things are done in parallel) */

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

    fp->f_this_fdrate = 0;
  
  } else if(fp->f_this_delay >= POCO_MAX_COARSE_DELAY){
    fp->f_this_delay = POCO_MAX_COARSE_DELAY - 1;
    fp->f_this_delta = 0;

    fp->f_this_fdrate = 0; 
    
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "coarse delay reached maximum of %d samples", fp->f_this_delay);
  } 

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "fixer %d loading coarse %u and fine %d", fp->f_id, fp->f_this_delay, fp->f_this_slope);

#if 0
  rr = read_pce(d, fp->f_status, &value, 0, 4);
  if(rr != 4){
    return -1;
  }

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "status is 0x%08x", value);


  if(value & COARSE_DELAY_PENDING_STATUS((0x1 << fp->f_id))){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "fixer %d already armed in status 0x%08x", fp->f_id, value);
  }
#endif

  value = (fp->f_this_mcount >> 32) & 0xffffffff;
  wr = write_pce(d, fp->f_register_time_msw, &value, 0, 4);
  if(wr != 4){
    return -1;
  }

  value = fp->f_this_mcount & 0xffffffff;
  wr = write_pce(d, fp->f_register_time_lsw, &value, 0, 4);
  if(wr != 4){
    return -1;
  }

#if 0
  int fudge = fp->f_this_fdrate / 10;
  /* TODO: load rate, not zero */
  value = (fp->f_this_slope & 0xffff) | (((fp->f_this_fdrate - fudge) << 16) & 0xffff0000);
  wr = write_pce(d, fp->f_register_fine_slope, &value, 0, 4);
  if(wr != 4){
    return -1;
  }
  
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "value: %x fdrate:%d slope:%d coarse:%d ", value, fp->f_this_fdrate, fp->f_this_slope,fp->f_this_delay);
  
  #if 1 
  /*value = (fp->f_this_offset & 0xffff) | ((fp->f_this_fdrate << 16) & 0xffff0000);*/
  value = (fp->f_this_offset & 0xffff) | ((0 << 16) & 0xffff0000);
  #endif
  wr = write_pce(d, fp->f_register_fine_offset, &value, 0, 4);
  if(wr != 4){
    return -1;
  }

#if 1 
  value = fp->f_this_delay;
  wr = write_pce(d, fp->f_register_coarse_value, &value, 0, 4);
  if(wr != 4){
    return -1;
  }
#endif
#endif


#if 0
  value = 0;
  wr = write_pce(d, fp->f_control, &value, 0, 4);
  if(wr != 4){
    return -1;
  }
  value = COARSE_DELAY_ARM_CONTROL((0x1 << fp->f_id)) | COARSE_CLEAR_MISS_CONTROL((0x1 << fp->f_id));
  wr = write_pce(d, fp->f_control, &value, 0, 4);
  if(wr != 4){
    return -1;
  }

  value = sp->p_source_state;
  wr = write_pce(d, fp->f_control0, &value, 0, 4);
  if(wr != 4){
    return -1;
  }
  /* clear errors and select source */
  value = sp->p_source_state | FINE_DELAY_ARM_CONTROL0((0x1 << fp->f_id)) | FINE_CLEAR_MISS_CONTROL0((0x1 << fp->f_id));
  wr = write_pce(d, fp->f_control0, &value, 0, 4);
  if(wr != 4){
    return -1;
  }

  rr = read_pce(d, fp->f_status, &value, 0, 4);
  if(rr != 4){
    return -1;
  }
  if(!(value & COARSE_DELAY_PENDING_STATUS((0x1 << fp->f_id)))){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "unable to arm coarse delay logic");
  }
  if((value & COARSE_DELAY_MISSED_STATUS((0x1 << fp->f_id)))){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "coarse delay loaded too late");
  }

  rr = read_pce(d, fp->f_status0, &value, 0, 4);
  if(rr != 4){
    return -1;
  }
  if(!(value & FINE_DELAY_PENDING_STATUS0((0x1 << fp->f_id)))){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "unable to arm fine delay logic");
  }
  if((value & FINE_DELAY_MISSED_STATUS0((0x1 << fp->f_id)))){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "fine delay loaded too late");
  }
#endif

  /* tick over */
  fp->f_this_delay += fp->f_this_delta;
  /*fp->f_this_mcount += (uint64_t)(sp->p_dsp_clock / USECPERSEC_POCO)* fp->f_this_add;*/
  fp->f_this_mcount += fp->f_this_add;

  log_message_katcp(d,KATCP_LEVEL_INFO,NULL,"this_add: %llu",fp->f_this_add);

  fp->f_this_slope = 0;
  fp->f_this_offset = 0;

  /* cut over to new values for next schedule */
  if(fp->f_fresh && (fp->f_this_mcount >= fp->f_stage_mcount)){
    log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "intending to cut over to new rate");

    fp->f_this_mcount = fp->f_stage_mcount;
    fp->f_this_add = fp->f_stage_add;

    fp->f_this_fdrate = fp->f_stage_fdrate;

    fp->f_this_delay = fp->f_stage_delay;
    fp->f_this_delta = fp->f_stage_delta;

    fp->f_this_slope = fp->f_stage_slope;
    fp->f_this_offset = fp->f_stage_offset;

    fp->f_fresh = 0;
  }
#if 1 
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
#endif
  return 0;
}

int schedule_fixer(struct katcp_dispatch *d, struct fixer_poco *fp)
{
  char buffer[NAME_BUFFER];
  struct timeval actual, headstart, delta;
  struct state_poco *sp;
  uint32_t value;

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

  snprintf(buffer, NAME_BUFFER - 1, LD_MSW_SKEL_POCO_REGISTER, fp->f_id);
  buffer[NAME_BUFFER - 1] = '\0';
  fp->f_register_time_msw = by_name_pce(d, buffer);

  snprintf(buffer, NAME_BUFFER - 1, LD_LSW_SKEL_POCO_REGISTER, fp->f_id);
  buffer[NAME_BUFFER - 1] = '\0';
  fp->f_register_time_lsw = by_name_pce(d, buffer);

  snprintf(buffer, NAME_BUFFER - 1, LD_STATUS_SKEL_POCO_REGISTER, fp->f_id);
  buffer[NAME_BUFFER - 1] = '\0';
  fp->f_register_time_status = by_name_pce(d, buffer);

  if((fp->f_register_time_msw == NULL) || 
     (fp->f_register_time_lsw == NULL) || 
     (fp->f_register_time_status == NULL)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to locate time registers");
    return -1;
  }

  snprintf(buffer, NAME_BUFFER - 1, COARSE_SKEL_POCO_REGISTER, fp->f_id);
  buffer[NAME_BUFFER - 1] = '\0';
  fp->f_register_coarse = by_name_pce(d, buffer);

  snprintf(buffer, NAME_BUFFER - 1, FINE_SKEL_POCO_REGISTER, fp->f_id);
  buffer[NAME_BUFFER - 1] = '\0';
  fp->f_register_fine = by_name_pce(d, buffer);

  snprintf(buffer, NAME_BUFFER - 1, PHASE_SKEL_POCO_REGISTER, fp->f_id);
  buffer[NAME_BUFFER - 1] = '\0';
  fp->f_register_phase = by_name_pce(d, buffer);

  if((fp->f_register_coarse == NULL) || 
     (fp->f_register_fine == NULL) || 
     (fp->f_register_phase == NULL)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to locate registers needed for delay values");
    return -1;
  }

  if(write_pce(d, fp->f_register_coarse, &(fp->f_packed_coarse), 0, 4) != 4){
    return -1;
  }
  if(write_pce(d, fp->f_register_fine, &(fp->f_packed_fine), 0, 4) != 4){
    return -1;
  }
  if(write_pce(d, fp->f_register_phase, &(fp->f_packed_phase), 0, 4) != 4){
    return -1;
  }

  value = fp->f_mcount & 0xffffffff;
  if(write_pce(d, fp->f_register_time_lsw, &value, 0, 4) != 4){
    return -1;
  }

  /* high bit to arm */
  value = ((fp->f_mcount >> 32) & 0xffffffff) | 0x80000000;
  if(write_pce(d, fp->f_register_time_msw, &value, 0, 4) != 4){
    return -1;
  }

  value = ((fp->f_mcount >> 32) & 0x7fffffff);
  if(write_pce(d, fp->f_register_time_msw, &value, 0, 4) != 4){
    return -1;
  }


#if 0
  snprintf(buffer, NAME_BUFFER - 1, FD_OFFSET_SKEL_POCO_REGISTER, fp->f_id);
  buffer[NAME_BUFFER - 1] = '\0';
  fp->f_register_fine_offset = by_name_pce(d, buffer);

  snprintf(buffer, NAME_BUFFER - 1, FD_SLOPE_SKEL_POCO_REGISTER, fp->f_id);
  buffer[NAME_BUFFER - 1] = '\0';
  fp->f_register_fine_slope = by_name_pce(d, buffer);

  if((fp->f_register_fine_offset == NULL) || 
     (fp->f_register_fine_slope == NULL)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to locate registers needed for fine delay");
    return -1;
  }
#endif

#if 0
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

    fp->f_this_fdrate = fp->f_stage_fdrate;
    
    fp->f_this_slope = fp->f_stage_slope;
    fp->f_this_offset = fp->f_stage_offset + fp->f_base_offset; /* TODO:  add base_offset here */

    mcount_to_tv_poco(d, &actual, &(fp->f_stage_mcount));

    delta.tv_sec = 0;
    delta.tv_usec = sp->p_lead;

    sub_time_katcp(&headstart, &actual, &delta);

#if 0
    if(register_at_tv_katcp(d, &headstart, &run_fixer, fp) < 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to register timer");
      return -1;
    }
#endif

    fp->f_up = 1;
  } else {
    /* the next_* should really be parameters to schedule, then the fresh flag is safer */
    fp->f_fresh = 1;
  }
#endif

  return 0;
}
