#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <loop.h>
#include <katcp.h>

#include "multiserver.h"

/****************************************************/

struct mul_client *create_client(struct list_loop *ls)
{
  struct mul_client *ci;

  ci = malloc(sizeof(struct mul_client));
  if(ci == NULL){
    return NULL;
  }

#ifdef DEBUG
  ci->c_magic = MUL_MAGIC;
#endif

  ci->c_dispatch = NULL;

  ci->c_task = NULL;
  ci->c_loop = ls;

  ci->c_overall = global_loop(ls);

  ci->c_save = NULL;
  ci->c_xlookup = NULL;

  ci->c_waiting = 0;
  ci->c_queue = 0;

  return ci;
}

void *setup_client(struct list_loop *ls, void *data, struct sockaddr *addr, int fd)
{
  struct mul_client *ci;
  struct katcp_dispatch *d;

  ci = create_client(ls);
  if(ci == NULL){
    return NULL;
  }

  ci->c_dispatch = clone_katcp(d);
  if(ci->c_dispatch == NULL){
    shutdown_client(ci);
    return NULL;
  }

  set_multi_katcp(ci->c_dispatch, ci);
  exchange_katcp(ci->c_dispatch, fd);
  on_connect_katcp(ci->c_dispatch);

  return ci;
}

void shutdown_client(struct mul_client *ci)
{
  if(ci == NULL){
    return;
  }

#ifdef DEBUG
  if(ci->c_magic != MUL_MAGIC){
    fprintf(stderr, "shutdown client: bad client magic\n");
    abort();
  }
#endif

  if(ci->c_xlookup){
    destroy_xlookup(ci->c_xlookup);
    ci->c_xlookup = NULL;
  }

  if(ci->c_dispatch){
    shutdown_katcp(ci->c_dispatch);
    ci->c_dispatch = NULL;
  }

  free(ci);
}

int run_client(struct list_loop *ls, struct task_loop *tl, int mask)
{
  struct mul_client *ci;
  int update, idle, result;
#if 0
  struct task_loop *tx;
#endif
  struct link_loop *lk;
  struct mul_msg *mm;
  
  ci = user_loop(ls, tl);
  if(ci == NULL){
    stop_loop(ls, tl);
    return 0;
  }

  ci->c_task = tl;

  idle = LO_STOP_MASK | LO_WAIT_MASK;

#ifdef DEBUG
  fprintf(stderr, "client[%p]: mask 0x%02x\n", tl, mask);
#endif

  update = idle;
  if(mask & LO_STOP_MASK){
    /* WARNING: may have to remove fd to prevent almost harmless but ugly double close */
    shutdown_client(ci);
    return 0;
  }

  /* handle read case */
  update |= LO_READ_MASK;
  if(mask & LO_READ_MASK){
    result = read_katcp(ci->c_dispatch);
#ifdef DEBUG
    fprintf(stderr, "client: read code is %d\n", result);
#endif
    if(result < 0){
      stop_loop(ls, tl);
      return 0;
    }
    if(result > 0){
      update &= ~(LO_READ_MASK); /* on EOF stop reading */
    }
  }

  /* handle work function */
  result = lookup_katcp(ci->c_dispatch);
  if(result < 0){
    stop_loop(ls, tl);
    return 0;
  }

  if(result > 0){
    /* WARNING: statement triggers dispatch if no request pending (new) or if something has arrived (with pending request) */
    if((ci->c_waiting == 0) || (mask & LO_WAIT_MASK)){
#ifdef DEBUG
      fprintf(stderr, "client[%p]: calling dispatch (queue=%d)\n", tl, ci->c_queue);
#endif
      result = call_katcp(ci->c_dispatch);
      if(result == KATCP_RESULT_RESUME){
        ci->c_waiting = (ci->c_queue > 0) ? 1 : 0;
      } else {
        ci->c_waiting = 0;
        if(ci->c_queue > 0){ /* WARNING: this should actually be an abort, as the dispatch routines have broken the queue */
          fprintf(stderr, "client[%p]: warning: finished with %d outstanding requests\n", tl, ci->c_queue);
          log_message_katcp(ci->c_dispatch, KATCP_LEVEL_ERROR, NULL, "client function has %d outstand requests", ci->c_queue);
        }
        ci->c_queue = 0;
      }
    }
  }

  if(mask & LO_WAIT_MASK){
    if(!(ci->c_waiting)){ /* no dispatch function busy, so we handle the informs and ditch everything else */
#ifdef DEBUG
      fprintf(stderr, "client[%p]: idle, handling queue\n", tl);
#endif
      while((lk = receive_link_loop(ls, tl)) != NULL){
#ifdef DEBUG
        fprintf(stderr, "client[%p]: received event type %d\n", tl, lk->k_type);
#endif
        if((lk->k_type == ci->c_overall->o_type_msg) && (lk->k_data != NULL)){
          mm = lk->k_data;
          if(arg_inform_msg(mm)){ /* inform messages get sent out */
            dispatch_from_msg(ci->c_dispatch, mm);
          }
        }
        discard_link_loop(ls, lk);
      } 
    }
  }

  /* handle write case */
  if(mask & LO_WRITE_MASK){
    if(write_katcp(ci->c_dispatch) < 0){
      stop_loop(ls, tl);
      return 0;
    }
  }

  if(flushing_katcp(ci->c_dispatch)){
    /* WARNING: could disable reads and running if flush buffer too large */
    update |= LO_WRITE_MASK;
  }

  if(update == idle){
    /* if we are only interested in stopping then stop */
    stop_loop(ls, tl);
  }

  set_mask_loop(ls, tl, update, 0);

  if(exited_katcp(ci->c_dispatch) != KATCP_EXIT_NOTYET){
    /* TODO: make it stop */
  }

  return 0;
}

/* xlookup routines ****************************************/

int run_client_xlookup(struct katcp_dispatch *d, int argc)
{
  struct mul_client *ci;

  ci = get_multi_katcp(d);
  if(ci == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "not a multiserver");
    return KATCP_RESULT_FAIL;
  }

  return run_xlookup(ci, argc);
}

int enter_client_xlookup(struct katcp_dispatch *d, struct xlookup_entry *table)
{
  struct mul_client *ci;

  if(table == NULL){
    return -1;
  }

  ci = get_multi_katcp(d);
  if(ci == NULL){
    return -1;
  }

  if(ci->c_xlookup == NULL){
    ci->c_xlookup = create_xlookup();
    if(ci->c_xlookup == NULL){
      return -1;
    }
  }

  if(prepare_xlookup(ci->c_xlookup, table, NULL)){
    return -1;
  }

  continue_katcp(d, 0, &run_client_xlookup);

  return run_xlookup(ci, arg_count_katcp(d));
}


