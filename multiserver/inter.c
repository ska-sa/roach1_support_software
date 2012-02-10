#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <loop.h>

#include "multiserver.h"

int run_intermediate_xlookup(struct list_loop *ls, struct task_loop *tl, int mask)
{
  struct mul_client *ci;
  int result;

  ci = user_loop(ls, tl);
  if(ci == NULL){
    stop_loop(ls, tl);
    return 0;
  }

  if(mask & LO_STOP_MASK){
    shutdown_client(ci);
    return 0;
  }

  if(mask & LO_WAIT_MASK){
    result = run_xlookup(ci, 0);
    if(result != KATCP_RESULT_RESUME){
      stop_loop(ls, tl);
    }
  }

  return 0;
}

int start_inter_xlookup(struct list_loop *ls, struct xlookup_entry *table)
{
  struct mul_client *ci;
  struct task_loop *tl;
  struct overall_state *os;

  os = global_loop(ls);

  ci = create_client(ls);
  if(ci){
    ci->c_xlookup = create_xlookup();
    if(ci->c_xlookup){
      if(prepare_xlookup(ci->c_xlookup, table, NULL) == 0){
        tl = add_loop(ls, -1, ci, LO_STOP_MASK | LO_WAIT_MASK, &run_intermediate_xlookup, NULL, os->o_type_inter);
        if(tl){
          return run_xlookup(ci, 0);
        }
      }
    }
    shutdown_client(ci);
  }

  return -1;
}

