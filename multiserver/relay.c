#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <katcp.h>
#include <loop.h>

#include "multiserver.h"

#if 0
void clear_katmc(struct katcp_dispatch *d, char *target)
{
  ci = get_multi_katcp(d);
  if(ci == NULL){
    return NULL;
  }

  ci->c_save = NULL;
}
#endif

struct mul_msg *issue_katmc(struct katcp_dispatch *d, char *target)
{
  struct mul_client *ci;

  ci = get_multi_katcp(d);
  if(ci == NULL){
    return NULL;
  }

  return issue_katic(ci, target);
}

struct mul_msg *issue_katic(struct mul_client *ci, char *target)
{
  struct task_loop *tx;
  struct link_loop *lk;
  struct mul_leaf *lx;
  struct mul_msg *mm;

  lx = NULL;

  tx = ci->c_save;
  ci->c_save = NULL;

  while((tx = find_task_loop(ci->c_loop, ci->c_task, tx, ci->c_overall->o_type_leaf)) != NULL){
    lx = user_loop(ci->c_loop, tx);
    if(lx){
#ifdef DEBUG
      fprintf(stderr, "issue: checking <%s> against <%s>\n", lx->l_version, target);
#endif
      /* TODO: target comparisons could be more sophisticated, possibly comparing against other fields than just version */
      if(lx->l_version && (!strcmp(lx->l_version, target))){
        break;
      }
    }
    lx = NULL;
  }

  if(lx == NULL){
#ifdef DEBUG
    fprintf(stderr, "issue: no match for <%s>\n", target);
#endif
    return NULL;
  }

  lk = send_link_loop(ci->c_loop, ci->c_task, tx, ci->c_overall->o_type_msg);
#ifdef DEBUG
  fprintf(stderr, "issue[%p]: relaying inform to %p\n", ci->c_task, tx);
#endif

  if(lk == NULL){
#ifdef DEBUG
    fprintf(stderr, "issue: unable to send link\n");
#endif
    return NULL;
  }

  if(lk->k_type != ci->c_overall->o_type_msg){
    mm = create_msg();
    lk->k_data = mm;
    lk->k_type = ci->c_overall->o_type_msg;
  } else {
    mm = lk->k_data;
  }

  if(mm == NULL){
#ifdef DEBUG
    fprintf(stderr, "issue: no link data available to send link\n");
#endif
    return NULL;
  }

  /* make sure it is fresh */
  empty_msg(mm);

  ci->c_save = tx;
  ci->c_queue++;

  return mm;
}

/********************************/

void release_katmc(struct katcp_dispatch *d, struct mul_msg *mm)
{
  struct mul_client *ci;

  ci = get_multi_katcp(d);
  if(ci == NULL){
    return;
  }

  release_katic(ci, mm);
}

void release_katic(struct mul_client *ci, struct mul_msg *mm)
{
  struct link_loop *lk;

  lk = receive_link_loop(ci->c_loop, ci->c_task);
  if(lk == NULL){
    return;
  }

  if((lk->k_type != ci->c_overall->o_type_msg) || (lk->k_data == NULL)){
    discard_link_loop(ci->c_loop, lk);
    return;
  }

  if(mm != lk->k_data){
#ifdef DEBUG
    fprintf(stderr, "release: client logic failure: wanting to release something not at head of queue\n");
    abort();
#endif
    return;
  }

  discard_link_loop(ci->c_loop, lk);

#ifdef DEBUG
  fprintf(stderr, "release: about to decrement queue from %d\n", ci->c_queue);
#endif
  ci->c_queue--;
}

/*****************/

struct mul_msg *collect_katmc(struct katcp_dispatch *d)
{
  struct mul_client *ci;

  ci = get_multi_katcp(d);
  if(ci == NULL){
    return NULL;
  }

  return collect_katic(ci);
}

struct mul_msg *collect_katic(struct mul_client *ci)
{
  struct link_loop *lk;
  struct mul_msg *mm;

  for(;;){
    lk = receive_link_loop(ci->c_loop, ci->c_task);
    if(lk == NULL){
#ifdef DEBUG
      fprintf(stderr, "collect: queue empty, nothing there\n");
#endif
      return NULL;
    }

    if((lk->k_type != ci->c_overall->o_type_msg) || (lk->k_data == NULL)){
      discard_link_loop(ci->c_loop, lk);
      continue;
    }

    mm = lk->k_data;

    if(arg_reply_msg(mm)){
#ifdef DEBUG
      if(ci->c_queue <= 0){
        fprintf(stderr, "collect: logic problem, queue is %d\n", ci->c_queue);
      }
#endif
    }

    return mm; /* conundrum: do we send out log messages here, or let client functions handle them on a case by case basis ? For the time being let the clients see them */
  }
}

/*******************/

int outstanding_katmc(struct katcp_dispatch *d)
{
  struct mul_client *ci;

  ci = get_multi_katcp(d);
  if(ci == NULL){
    return 0;
  }

  return outstanding_katic(ci);
}

int outstanding_katic(struct mul_client *ci)
{
  return ci->c_queue;
}
