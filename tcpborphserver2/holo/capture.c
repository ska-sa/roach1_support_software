#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <arpa/inet.h>

#include <katpriv.h>

#include "core.h"
#include "poco.h"
#include "holo.h"
#include "modes.h"
#include "holo-options.h"

#define IDLE_GENERIC_CAPTURE    0 /* start condidtion: no prep time set */
#define PREP_GENERIC_CAPTURE    1 /* prep timer running */
#define START_GENERIC_CAPTURE   2 /* start timer running */
#define RUN_GENERIC_CAPTURE     3 /* working, possibly stop time set */

#define  CAPTURE_MAGIC 0x74755265

static int timer_generic_capture(struct katcp_dispatch *d, void *data);

#ifdef DEBUG
void sane_capture_holo(struct capture_holo *ch)
{
  if(ch == NULL){
    fprintf(stderr, "capture: ch is null\n");
    abort();
  }
  if(ch->c_magic != CAPTURE_MAGIC){
    fprintf(stderr, "capture: bad magic 0x%08x, expected 0x%08x\n", ch->c_magic, CAPTURE_MAGIC);
    abort();
  }
}
#else
#define sane_capture_holo(ch)
#endif

int run_generic_capture_holo(struct katcp_dispatch *d, struct capture_holo *ch, int poke)
{
  sane_capture_holo(ch);

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "capture %s with notification %d and state %d", ch->c_name, poke, ch->c_state);

  switch(poke){

    case CAPTURE_POKE_AUTO : /* automatic state machine runner */
      switch(ch->c_state){

        case IDLE_GENERIC_CAPTURE :
          log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "running capture timer while idle is unexpected");
          return -1;

        case PREP_GENERIC_CAPTURE :

          log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "capture-prepare %s", ch->c_name);

          if(ch->c_fd < 0){
            log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "no valid destination, not starting capture %s", ch->c_name);
            return -1;
          }
          if(ch->c_start.tv_sec == 0){
            log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "no valid start time while prepping");
            ch->c_state = IDLE_GENERIC_CAPTURE;
            return -1;
          }
          if(ch->c_toggle){
            log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "toggling start for %s", ch->c_name);

            if((*(ch->c_toggle))(d, ch->c_dump, 1) < 0){
              log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to trigger data collection");
              ch->c_state = IDLE_GENERIC_CAPTURE;
              return -1;
            }
          }
          register_at_tv_katcp(d, &(ch->c_start), &timer_generic_capture, ch);
          ch->c_start.tv_sec = 0;
          ch->c_start.tv_sec = 0;

          meta_udp_holo(d, ch, START_STREAM_CONTROL_HOLO);
          tx_udp_holo(d, ch);

          ch->c_state = START_GENERIC_CAPTURE;
          break;

        case START_GENERIC_CAPTURE : 
          /* MAYBE: change from log to #capture-start ? */
          log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "capture-start %s", ch->c_name);

          if(ch->c_stop.tv_sec){
            log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "scheduling stop at %lu.%06lu", ch->c_stop.tv_sec, ch->c_stop.tv_usec);
            register_at_tv_katcp(d, &(ch->c_stop), &timer_generic_capture, ch);
          }
          ch->c_state = RUN_GENERIC_CAPTURE;
          break;

        case RUN_GENERIC_CAPTURE  : 

          log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "capture-stop %s", ch->c_name);

          if(ch->c_toggle){
            if((*(ch->c_toggle))(d, ch->c_dump, 0) < 0){
              log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "problems stopping %s", ch->c_name);
            }
          } else {
            log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "no switch to stop capture for %s", ch->c_name);
          }

          meta_udp_holo(d, ch, STOP_STREAM_CONTROL_HOLO);
          tx_udp_holo(d, ch);

          ch->c_stop.tv_sec = 0;
          ch->c_stop.tv_usec = 0;
          ch->c_state = IDLE_GENERIC_CAPTURE;
          break;

        default :
          log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "bad state %d for capture %s", ch->c_state, ch->c_name);
          ch->c_state = IDLE_GENERIC_CAPTURE;
          return -1;
      }
      break;

    case CAPTURE_POKE_START : /* user notification start */

      switch(ch->c_state){
        case IDLE_GENERIC_CAPTURE :
          break;
        case PREP_GENERIC_CAPTURE :
          log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "overriding previously scheduled capture start for %s", ch->c_name);
          break;
        default :
          log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "capture for %s already running or scheduled to run", ch->c_name);
          return -1;
      }

      if(ch->c_prep.tv_sec == 0){
        log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "no valid prep time given for %s", ch->c_name);
        return -1;
      }

      if(cmp_time_katcp(&(ch->c_prep), &(ch->c_start)) >= 0){
        log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "prep time not before start time for %s", ch->c_name);
        return -1;
      }

      if((ch->c_stop.tv_sec != 0) && (cmp_time_katcp(&(ch->c_stop), &(ch->c_start)) < 0)){
        log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "overriding stale stop carelessly scheduled for %lu.%06lus", ch->c_stop.tv_sec, ch->c_stop.tv_usec);
        ch->c_stop.tv_sec = 0;
        ch->c_stop.tv_usec = 0;
      }

      if(ch->c_fd < 0){
        log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "hoping for that valid destination for %s will be set soon", ch->c_name);
      }

      register_at_tv_katcp(d, &(ch->c_prep), &timer_generic_capture, ch);

      ch->c_prep.tv_sec = 0;
      ch->c_prep.tv_usec = 0;

      ch->c_state = PREP_GENERIC_CAPTURE;
      break;

    case CAPTURE_POKE_STOP  : /* user notification - stop */
      if(ch->c_stop.tv_sec == 0){
        log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "having no stop time set when requesting sto is a major logic failure");
      }

      switch(ch->c_state){
        case IDLE_GENERIC_CAPTURE :
          if(ch->c_start.tv_sec != 0) {
            log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "having a start time set in idle state is a major logic failure");
          }

          log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "setting a stop at %lu.%lus while idle on %s despite having no start time", ch->c_stop.tv_sec, ch->c_stop.tv_usec, ch->c_name);
          break;

        case PREP_GENERIC_CAPTURE : 
          if(cmp_time_katcp(&(ch->c_stop), &(ch->c_start)) <= 0){

            log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "cancelling capture on %s as stop time preceeds start time", ch->c_name);

            ch->c_stop.tv_sec = 0;
            ch->c_stop.tv_usec = 0;

            ch->c_start.tv_sec = 0;
            ch->c_start.tv_usec = 0;

            discharge_timer_katcp(d, ch);
          }
          break;

        case RUN_GENERIC_CAPTURE :
          register_at_tv_katcp(d, &(ch->c_stop), &timer_generic_capture, ch);
          break;

      }
      break;

    default :
      log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "unknown poke state %d", poke);
      return -1;
      break;
  }

  return 0; /* time registration functions look at this code, but only for periodic events */
}

static int timer_generic_capture(struct katcp_dispatch *d, void *data)
{
  struct capture_holo *ch;

  ch = data;
  
  sane_capture_holo(ch);

#ifdef DEBUG
  if(ch->c_schedule != &run_generic_capture_holo){
    fprintf(stderr, "capture: major logic problem: automatic schedule function %p does not generic capture %p\n", ch->c_schedule, &run_generic_capture_holo);
    abort();
  }
#endif

  return run_generic_capture_holo(d, ch, CAPTURE_POKE_AUTO);
}

int register_capture_holo(struct katcp_dispatch *d, char *name, int (*call)(struct katcp_dispatch *d, struct capture_holo *ch, int poke))
{
  struct state_holo *sh;
  struct capture_holo *ch, **tmp;

  sh = get_mode_katcp(d, POCO_HOLO_MODE);
  if(sh == NULL){
    return -1;
  }

  tmp = realloc(sh->h_captures, sizeof(struct capture_holo *) * (sh->h_size + 1));
  if(tmp == NULL){
    return -1;
  }

  sh->h_captures = tmp;

  ch = malloc(sizeof(struct capture_holo));
  if(ch == NULL){
    return -1;
  }

  ch->c_magic = CAPTURE_MAGIC;

  ch->c_name = NULL;
  ch->c_fd = (-1);

  ch->c_port = htons(0);
  ch->c_ip = htonl(0);

  /* c_prep - unset */

  ch->c_start.tv_sec = 0;
  ch->c_start.tv_usec = 0;

  ch->c_stop.tv_sec = 0;
  ch->c_stop.tv_usec = 0;

#if 0
  ch->c_period.tv_sec = 0;
  ch->c_period.tv_usec = 0;
#endif

  ch->c_state = 0;/*TO DO:Do we need this*/
#if 0
  ch->c_ping = 0;
#endif
  ch->c_schedule = call;

  ch->c_ts_msw = 0;
  ch->c_ts_lsw = 0;
  ch->c_options = 0;

  ch->c_buffer = NULL;
  ch->c_size = 0;
  ch->c_sealed = 0;
  ch->c_limit = MTU_POCO - PACKET_OVERHEAD_POCO;
  ch->c_used = 8; /* always have a header */

  ch->c_failures = 0;

  /*TO DO:To check this*/
  ch->c_dump = NULL;
  ch->c_toggle = NULL;
  ch->c_destroy = NULL;

  ch->c_name = strdup(name);
  if(ch->c_name == NULL){
    destroy_capture_holo(d, ch);
    return -1;
  }

  ch->c_buffer = malloc(ch->c_limit);
  if(ch->c_buffer == NULL){
    destroy_capture_holo(d, ch);/*TO DO: Fill this*/
    return -1;
  }
  ch->c_size = ch->c_limit - DATA_OVERHEAD_POCO;

  sh->h_captures[sh->h_size] = ch;
  sh->h_size++;

  return 0;
}

void destroy_capture_holo(struct katcp_dispatch *d, struct capture_holo *ch)
{
  if(ch == NULL){
    return;
  }

  sane_capture_holo(ch);

  if(ch->c_fd >= 0){
    close(ch->c_fd);
    ch->c_fd = (-1);
  }

  if(ch->c_name){
    free(ch->c_name);
    ch->c_name = NULL;
  }

  if(ch->c_buffer){
    free(ch->c_buffer);
    ch->c_buffer = NULL;
  }

  ch->c_state = 0;
#if 0
  ch->c_ping = 0;
#endif

  ch->c_size = 0;

  ch->c_destroy = NULL;
  ch->c_toggle = NULL;

  free(ch);
}


int register_bram_holo(struct katcp_dispatch *d, char *name, unsigned int blocks)
{
    struct capture_holo *ch;
    struct state_holo *sh;

    sh = get_mode_katcp(d, POCO_HOLO_MODE);
    if(sh == NULL){
#ifdef DEBUG
        fprintf(stderr, "bram: unable to access mode %d\n", POCO_HOLO_MODE);
#endif
        return -1;
    }

    /*If FN unchanged can use by including poco.h*/
    if(register_capture_holo(d, name, &run_generic_capture_holo)){
#ifdef DEBUG
        fprintf(stderr, "bram: unable to register capture\n");
#endif
        return -1;
    }

    ch = find_capture_holo(sh, name);
#ifdef DEBUG
    if(ch == NULL){
        fprintf(stderr, "bram: major logic failure: unable to find freshly created instance %s\n", name);
        abort();
    }
#endif

    sane_capture_holo(ch);

#if 0
    /* TODO: make this a parameter */
    ch->c_period.tv_sec  = 1;
    ch->c_period.tv_usec = 0;
#endif

    /*TO DO: IS this required for bram like dram??*/
    if(create_bram_holo(d, ch, blocks)){
#ifdef DEBUG
        fprintf(stderr, "bram: unable to create bram\n");
#endif
        return -1;
    }
#ifdef DEBUG
    fprintf(stderr, "register_capture_holo fn registered\n");
#endif


    return 0;
}
