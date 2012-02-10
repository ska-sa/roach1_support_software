#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "multiserver.h"

struct xlookup_state *create_xlookup(void)
{
  struct xlookup_state *xs;

  xs = malloc(sizeof(struct xlookup_state));
  if(xs == NULL){
    return NULL;
  }

  xs->s_table = NULL;
  xs->s_current = 0;
  xs->s_size = 0;

  return xs;
}

void destroy_xlookup(struct xlookup_state *xs)
{
  if(xs == NULL){
    return;
  }

  xs->s_table = NULL;
  xs->s_current = 0;
  xs->s_size = 0;

  free(xs);
}

int prepare_xlookup(struct xlookup_state *xs, struct xlookup_entry *table, char *extra)
{
  int i;
  struct xlookup_entry *xe;

  xs->s_table = table;
  xs->s_current = 0;
  xs->s_size = 0;

  /* do basic check for valid table */
  for(xs->s_size = 0; (xs->s_table[xs->s_size].x_issue || xs->s_table[xs->s_size].x_collect) ; xs->s_size++);

  if(xs->s_size <= 0){
    return -1;
  }

  for(i = 0; i < xs->s_size; i++){
    xe = &(xs->s_table[i]);

    if((xe->x_success >= xs->s_size) || (xe->x_failure >= xs->s_size)){
#ifdef DEBUG
      fprintf(stderr, "prepare: table invalid at entry %d\n", i);
#endif
      return -1;
    }

    /* possibly remove this test, if there is a busy state */
    if(xe->x_issue && (xe->x_target == NULL)){
#ifdef DEBUG
      fprintf(stderr, "prepare: need a target to issue messages\n");
#endif
      return -1;
    }

    if(xe->x_issue && xe->x_collect){
#ifdef DEBUG
      fprintf(stderr, "prepare: have both issue and collect in a single state\n");
#endif
      return -1;
    }

    if(!(xe->x_issue || xe->x_collect)){
#ifdef DEBUG
      fprintf(stderr, "prepare: have neither issue and collect\n");
#endif
      return -1;
    }
  }

  xs->s_repeats = 0;
  xs->s_status = 0;

  return 0;
}

int run_xlookup(struct mul_client *ci, int argc)
{
  struct xlookup_state *xs;
  struct xlookup_entry *xe;
  struct mul_msg *mm;
  int result, check, count, run;

  if(ci->c_xlookup == NULL){
    if(ci->c_dispatch){
      log_message_katcp(ci->c_dispatch, KATCP_LEVEL_ERROR, NULL, "not xlookup logic available");
    }
    return KATCP_RESULT_FAIL;
  }

  xs = ci->c_xlookup;
  xe = &(xs->s_table[xs->s_current]);

  for(run = 1; run; ){

#ifdef DEBUG
    fprintf(stderr, "xlabel: run in state %d, target is <%s>\n", xs->s_current, xe->x_target);
#endif

    /* run through table */
    if(xe->x_target){ 

      /* WARNING: intricate logic, failures path has greater
       * priority than success than stay, but we try to send
       * to everybody */
      count = 0;
      result = 0;
      while((mm = issue_katic(ci, xe->x_target)) != NULL){
        count++;
        check = (*(xe->x_issue))(ci, mm);
        if(check < 0){
          result = (-1); /* a local failure forces a global failure */
        } else if(result == 0){ /* if in global stay state can advance to success */
          result = check;
        } /* else global failure or success stay if local stay or success */
      }
      if(count == 0){
        result = -1; /* by default chose fail path if no target, could also do success, but stay may lock up */
      }

    } else { /* do receive */
      mm = collect_katic(ci);
      if(mm == NULL){
        if(ci->c_dispatch){
          log_message_katcp(ci->c_dispatch, KATCP_LEVEL_ERROR, NULL, "unable to collect message in state %d", xs->s_current);
        }
        return KATCP_RESULT_FAIL;
      }
      result = (*(xe->x_collect))(ci, mm);
      release_katic(ci, mm);
    }

    if(result){
      if(ci->c_dispatch){
        log_message_katcp(ci->c_dispatch, KATCP_LEVEL_DEBUG, NULL, "%s transition %d->%d", 
            (result > 0) ? "success" : "fail", 
            xs->s_current, 
            (result > 0) ? xe->x_success : xe->x_failure);
      }

      xs->s_current = (result > 0) ? xe->x_success : xe->x_failure;
      xs->s_repeats = 0;
    } else{
      xs->s_repeats++;
      if(ci->c_dispatch){

        log_message_katcp(ci->c_dispatch, KATCP_LEVEL_DEBUG, NULL, "remaining in state %d", xs->s_current);
      }
    }

#ifdef DEBUG
    fprintf(stderr, "xlookup: switch to state %d\n", xs->s_current);
#endif

    run = 0; /* assume we are going to exit */

    if(xs->s_current >= 0){
      xe = &(xs->s_table[xs->s_current]); /* has to be set for next iteration */
      if(xe->x_target || collect_katic(ci)){ /* continue if we can issue stuff or if there is stuff to receive */
        run = 1; 
      }
    }
  }

  if(xs->s_current >= 0){ /* still in state machine */
    return KATCP_RESULT_RESUME;
  } 

  /* could be a fancy macro to translate between KATCP_RESULT and XLK_LABEL */
  switch(xs->s_current){
    case XLK_LABEL_OK :
      return KATCP_RESULT_OK;
    default :
      return KATCP_RESULT_FAIL;
  }
}

/*************************************************************************/

int generic_collect_xlookup(struct mul_client *ci, struct mul_msg *mm)
{
  char *status;
  int result;
  struct xlookup_state *xs;

  if(ci->c_xlookup == NULL){
    return -1;
  }

  xs = ci->c_xlookup;

  if(!xs->s_repeats){
    xs->s_status = 1;
  }

  status = arg_string_msg(mm, 1);
  if((status == NULL) || strcmp(status, KATCP_OK)){
    xs->s_status = (-1);
  }

  if(outstanding_katic(ci) > 1){
    return 0;
  }

  result = xs->s_status;

#ifdef DEBUG
  xs->s_status = 0; /* clear things to trap errors */
  if(result == 0){
    fprintf(stderr, "collect: major logic problem: stuck\n");
    return -1;
  }
#endif

  return result;
}
