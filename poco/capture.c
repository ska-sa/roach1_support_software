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
#include "modes.h"
#include "options.h"

#define IDLE_GENERIC_CAPTURE    0 /* start condidtion: no prep time set */
#define PREP_GENERIC_CAPTURE    1 /* prep timer running */
#define START_GENERIC_CAPTURE   2 /* start timer running */
#define RUN_GENERIC_CAPTURE     3 /* working, possibly stop time set */

#define  CAPTURE_MAGIC 0x74755265

static int timer_generic_capture(struct katcp_dispatch *d, void *data);

#ifdef DEBUG
void sane_capture(struct capture_poco *cp)
{
  if(cp == NULL){
    fprintf(stderr, "capture: cp is null\n");
    abort();
  }
  if(cp->c_magic != CAPTURE_MAGIC){
    fprintf(stderr, "capture: bad magic 0x%08x, expected 0x%08x\n", cp->c_magic, CAPTURE_MAGIC);
    abort();
  }
}
#else
#define sane_capture(cp)
#endif

/* bit of a hack function - gets run by scheduler *AND* invoked - poke nonzero indicates manual run */

int run_generic_capture(struct katcp_dispatch *d, struct capture_poco *cp, int poke)
{
  /* logic:  */
  /* zero poke means that we are being run from a timer, advance our state */
  /* machine, by looking at state value (initial value zero, but never changed */
  /* by subsequent api logic */
  /* nonzero poke means that the user has changed a parameter */

#if 0
  /* scary logic for client using the API: ping means that the user has changed some parameter - client has to figure out what to do on the basis of that, otherwise just run whatever client state machine, client can use c_state for that, where 0 is the starting condition (never gets reset by API logic, client responsible for that) */
#endif

  sane_capture(cp);

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "capture %s with notification %d and state %d", cp->c_name, poke, cp->c_state);

  switch(poke){

    case CAPTURE_POKE_AUTO : /* automatic state machine runner */
      switch(cp->c_state){

        case IDLE_GENERIC_CAPTURE :
          log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "running capture timer while idle is unexpected");
          return -1;

        case PREP_GENERIC_CAPTURE :

          log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "capture-prepare %s", cp->c_name);

          if(cp->c_fd < 0){
            log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "no valid destination, not starting capture %s", cp->c_name);
            return -1;
          }
          if(cp->c_start.tv_sec == 0){
            log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "no valid start time while prepping");
            cp->c_state = IDLE_GENERIC_CAPTURE;
            return -1;
          }
          if(cp->c_toggle){
            log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "toggling start for %s", cp->c_name);

            if((*(cp->c_toggle))(d, cp->c_dump, 1) < 0){
              log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to trigger data collection");
              cp->c_state = IDLE_GENERIC_CAPTURE;
              return -1;
            }
          }
          register_at_tv_katcp(d, &(cp->c_start), &timer_generic_capture, cp);
          cp->c_start.tv_sec = 0;
          cp->c_start.tv_sec = 0;

          meta_udp_poco(d, cp, START_STREAM_CONTROL_POCO);
          tx_udp_poco(d, cp);

          cp->c_state = START_GENERIC_CAPTURE;
          break;

        case START_GENERIC_CAPTURE : 
          /* MAYBE: change from log to #capture-start ? */
          log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "capture-start %s", cp->c_name);

          if(cp->c_stop.tv_sec){
            log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "scheduling stop at %lu.%06lu", cp->c_stop.tv_sec, cp->c_stop.tv_usec);
            register_at_tv_katcp(d, &(cp->c_stop), &timer_generic_capture, cp);
          }
          cp->c_state = RUN_GENERIC_CAPTURE;
          break;

        case RUN_GENERIC_CAPTURE  : 

          log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "capture-stop %s", cp->c_name);

          if(cp->c_toggle){
            if((*(cp->c_toggle))(d, cp->c_dump, 0) < 0){
              log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "problems stopping %s", cp->c_name);
            }
          } else {
            log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "no switch to stop capture for %s", cp->c_name);
          }

          meta_udp_poco(d, cp, STOP_STREAM_CONTROL_POCO);
          tx_udp_poco(d, cp);

          cp->c_stop.tv_sec = 0;
          cp->c_stop.tv_usec = 0;
          cp->c_state = IDLE_GENERIC_CAPTURE;
          break;

        default :
          log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "bad state %d for capture %s", cp->c_state, cp->c_name);
          cp->c_state = IDLE_GENERIC_CAPTURE;
          return -1;
      }
      break;

    case CAPTURE_POKE_START : /* user notification start */

      switch(cp->c_state){
        case IDLE_GENERIC_CAPTURE :
          break;
        case PREP_GENERIC_CAPTURE :
          log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "overriding previously scheduled capture start for %s", cp->c_name);
          break;
        default :
          log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "capture for %s already running or scheduled to run", cp->c_name);
          return -1;
      }

      if(cp->c_prep.tv_sec == 0){
        log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "no valid prep time given for %s", cp->c_name);
        return -1;
      }

      if(cmp_time_katcp(&(cp->c_prep), &(cp->c_start)) >= 0){
        log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "prep time not before start time for %s", cp->c_name);
        return -1;
      }

      if((cp->c_stop.tv_sec != 0) && (cmp_time_katcp(&(cp->c_stop), &(cp->c_start)) < 0)){
        log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "overriding stale stop carelessly scheduled for %lu.%06lus", cp->c_stop.tv_sec, cp->c_stop.tv_usec);
        cp->c_stop.tv_sec = 0;
        cp->c_stop.tv_usec = 0;
      }

      if(cp->c_fd < 0){
        log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "hoping for that valid destination for %s will be set soon", cp->c_name);
      }

      register_at_tv_katcp(d, &(cp->c_prep), &timer_generic_capture, cp);

      cp->c_prep.tv_sec = 0;
      cp->c_prep.tv_usec = 0;

      cp->c_state = PREP_GENERIC_CAPTURE;
      break;

    case CAPTURE_POKE_STOP  : /* user notification - stop */
      if(cp->c_stop.tv_sec == 0){
        log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "having no stop time set when requesting sto is a major logic failure");
      }

      switch(cp->c_state){
        case IDLE_GENERIC_CAPTURE :
          if(cp->c_start.tv_sec != 0) {
            log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "having a start time set in idle state is a major logic failure");
          }

          log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "setting a stop at %lu.%lus while idle on %s despite having no start time", cp->c_stop.tv_sec, cp->c_stop.tv_usec, cp->c_name);
          break;

        case PREP_GENERIC_CAPTURE : 
          if(cmp_time_katcp(&(cp->c_stop), &(cp->c_start)) <= 0){

            log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "cancelling capture on %s as stop time preceeds start time", cp->c_name);

            cp->c_stop.tv_sec = 0;
            cp->c_stop.tv_usec = 0;

            cp->c_start.tv_sec = 0;
            cp->c_start.tv_usec = 0;

            discharge_timer_katcp(d, cp);
          }
          break;

        case RUN_GENERIC_CAPTURE :
          register_at_tv_katcp(d, &(cp->c_stop), &timer_generic_capture, cp);
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
  struct capture_poco *cp;

  cp = data;
  
  sane_capture(cp);

#ifdef DEBUG
  if(cp->c_schedule != &run_generic_capture){
    fprintf(stderr, "capture: major logic problem: automatic schedule function %p does not generic capture %p\n", cp->c_call, &run_generic_caputre);
    abort();
  }
#endif

  return run_generic_capture(d, cp, CAPTURE_POKE_AUTO);
}

int register_capture_poco(struct katcp_dispatch *d, char *name, int (*call)(struct katcp_dispatch *d, struct capture_poco *cp, int poke))
{
  struct state_poco *sp;
  struct capture_poco *cp, **tmp;

  sp = get_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return -1;
  }

  tmp = realloc(sp->p_captures, sizeof(struct capture_poco *) * (sp->p_size + 1));
  if(tmp == NULL){
    return -1;
  }

  sp->p_captures = tmp;
  
  cp = malloc(sizeof(struct capture_poco));
  if(cp == NULL){
    return -1;
  }

  cp->c_magic = CAPTURE_MAGIC;

  cp->c_name = NULL;
  cp->c_fd = (-1);

  cp->c_port = htons(0);
  cp->c_ip = htonl(0);

  /* c_prep - unset */

  cp->c_start.tv_sec = 0;
  cp->c_start.tv_usec = 0;

  cp->c_stop.tv_sec = 0;
  cp->c_stop.tv_usec = 0;

#if 0
  cp->c_period.tv_sec = 0;
  cp->c_period.tv_usec = 0;
#endif

  cp->c_state = 0;
#if 0
  cp->c_ping = 0;
#endif

  cp->c_schedule = call;

  cp->c_ts_msw = 0;
  cp->c_ts_lsw = 0;
  cp->c_options = 0;

  cp->c_buffer = NULL;
  cp->c_size = 0;
  cp->c_sealed = 0;
  cp->c_limit = MTU_POCO - PACKET_OVERHEAD_POCO;
  cp->c_used = 8; /* always have a header */

  cp->c_failures = 0;

  cp->c_dump = NULL;
  cp->c_toggle = NULL;
  cp->c_destroy = NULL;

  cp->c_name = strdup(name);
  if(cp->c_name == NULL){
    destroy_capture_poco(d, cp);
    return -1;
  }

  cp->c_buffer = malloc(cp->c_limit);
  if(cp->c_buffer == NULL){
    destroy_capture_poco(d, cp);
    return -1;
  }
  cp->c_size = cp->c_limit;

  sp->p_captures[sp->p_size] = cp;
  sp->p_size++;

  return 0;
}

void destroy_capture_poco(struct katcp_dispatch *d, struct capture_poco *cp)
{
  if(cp == NULL){
    return;
  }

  sane_capture(cp);

  if(cp->c_fd >= 0){
    close(cp->c_fd);
    cp->c_fd = (-1);
  }

  if(cp->c_name){
    free(cp->c_name);
    cp->c_name = NULL;
  }

  if(cp->c_buffer){
    free(cp->c_buffer);
    cp->c_buffer = NULL;
  }

  cp->c_state = 0;
#if 0
  cp->c_ping = 0;
#endif

  cp->c_size = 0;

  if(cp->c_dump){
#ifdef DEBUG
    if(cp->c_destroy == NULL){
      fprintf(stderr, "major logic problem: have dump state but nothing to release it\n");
      abort();
    }
#endif

    (*(cp->c_destroy))(d, cp->c_dump);
    cp->c_dump = NULL;
  }

  cp->c_destroy = NULL;
  cp->c_toggle = NULL;

  free(cp);
}

#if 0
int register_snap_poco(struct katcp_dispatch *d, char *name, char *prefix, int count, unsigned int blocks)
{
  struct capture_poco *cp;
  struct state_poco *sp;

  sp = get_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
#ifdef DEBUG
    fprintf(stderr, "snap: unable to access mode %d\n", POCO_POCO_MODE);
#endif
    return -1;
  }

  if(register_capture_poco(d, name, &run_generic_capture)){
#ifdef DEBUG
    fprintf(stderr, "snap: unable to register capture\n");
#endif
    return -1;
  }

  cp = find_capture_poco(sp, name);
#ifdef DEBUG
  if(cp == NULL){
    fprintf(stderr, "snap: major logic failure: unable to find freshly created instance %s\n", name);
    abort();
  }
#endif

  sane_capture(cp);

  /* TODO: make this a parameter */
#if 0
  cp->c_period.tv_sec  = 1;
  cp->c_period.tv_usec = 0;
#endif

  if(create_snap_poco(d, cp, prefix, count, blocks)){
#ifdef DEBUG
    fprintf(stderr, "snap: unable to create snap\n");
#endif
    return -1;
  }

  return 0;
}
#endif

int register_dram_poco(struct katcp_dispatch *d, char *name, unsigned int count)
{
  struct capture_poco *cp;
  struct state_poco *sp;

  sp = get_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
#ifdef DEBUG
    fprintf(stderr, "dram: unable to access mode %d\n", POCO_POCO_MODE);
#endif
    return -1;
  }

  if(register_capture_poco(d, name, &run_generic_capture)){
#ifdef DEBUG
    fprintf(stderr, "dram: unable to register capture\n");
#endif
    return -1;
  }

  cp = find_capture_poco(sp, name);
#ifdef DEBUG
  if(cp == NULL){
    fprintf(stderr, "dram: major logic failure: unable to find freshly created instance %s\n", name);
    abort();
  }
#endif

  sane_capture(cp);

#if 0
  /* TODO: make this a parameter */
  cp->c_period.tv_sec  = 1;
  cp->c_period.tv_usec = 0;
#endif

  if(create_dram_poco(d, cp, count)){
#ifdef DEBUG
    fprintf(stderr, "dram: unable to create dram\n");
#endif
    return -1;
  }

  return 0;
}

