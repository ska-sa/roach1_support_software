#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <katcp.h>
#include <katpriv.h> /* for timeval arith */

#include "poco.h"
#include "misc.h"
#include "config.h"
#include "registers.h"
#include "modes.h"

/* needed for meta packet, unclear if we should dive into that here */
#include "options.h"

/* I really wanted to do without uint64_t, but had to cave in here */
int mcount_to_tv_poco(struct katcp_dispatch *d, struct timeval *tv, uint64_t *count)
{
  struct timeval tmp;
  struct state_poco *sp;

  sp = get_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return -1;
  }

  /* WARNING: assumes that dsp_clock is a nice multiple of 1M */
  tmp.tv_sec  = (*count) / sp->p_dsp_clock; 
  tmp.tv_usec = ((*count) % sp->p_dsp_clock) / (sp->p_dsp_clock / USECPERSEC_POCO);

#if 0
  add_time_katcp(tv, &tmp, &(sp->p_sync_time));
#endif
  /* WARNING: assumes that sync happens on the second, otherwise use the above */
  tv->tv_sec  = tmp.tv_sec + sp->p_sync_time.tv_sec;
  tv->tv_usec = tmp.tv_usec;

  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "mcount %llu@%uHz -> %lu.%06lus", *count, sp->p_dsp_clock, tmp.tv_sec, tmp.tv_usec);

  return 0;
}

int tve_to_mcount_poco(struct katcp_dispatch *d, uint64_t *count, struct timeval *tv, unsigned int extra)
{
  unsigned long dsp_per_usec;
  struct timeval delta;
  struct state_poco *sp;
  uint64_t tmp;

  sp = get_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return -1;
  }

  dsp_per_usec = sp->p_dsp_clock / USECPERSEC_POCO;

  /* check for time before last sync */
  if((tv->tv_sec < sp->p_sync_time.tv_sec) || (sp->p_sync_time.tv_usec > 0)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "requested time preceeds time of sync");
    return -1;
  }

  if(sp->p_sync_time.tv_usec != 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "sync time apparently not on the second but offset by %luus", sp->p_sync_time.tv_usec);
  }

  /* WARNING: assumes that sync happens on the second */
  delta.tv_sec  = tv->tv_sec - sp->p_sync_time.tv_sec;
  delta.tv_usec = tv->tv_usec;

  /* expand subsecond parts to femto seconds, then convert to cycles */
  tmp = (extra + ((uint64_t)(delta.tv_usec) * 1000000000) * dsp_per_usec) / 1000000000;
  /* add the seconds back in, as clean multiples of cycles */
  tmp += (sp->p_dsp_clock * (uint64_t)(delta.tv_sec));

  *count = tmp;

  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "mcount %lu.%06lus -> %llu@%uHz", delta.tv_sec, delta.tv_usec, tmp, sp->p_dsp_clock);

  /* check if too much precision given */
  tmp = (uint64_t)dsp_per_usec * extra;
  if(tmp & 0xffffffff){
    return 1;
  }

  return 0;
}

