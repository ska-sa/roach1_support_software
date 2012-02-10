#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <sys/time.h>

#include <loop.h>
#include <katcl.h>

#include "multiserver.h"

static void stop_leaf(struct mul_leaf *lf);

void *setup_leaf(struct list_loop *ls, void *data, struct sockaddr *addr)
{
  struct mul_leaf *lf;

  lf = data;

  /* could remember the client IP here, or something */

  return NULL;
}

int broadcast_reply(struct list_loop *ls, struct task_loop *tl)
{
  struct mul_leaf *lf;
  struct link_loop *lk;
  struct overall_state *os;
  struct task_loop *tx;
  struct mul_msg *mm;
  int result;

  /* sanity checks happen in run */
  os = global_loop(ls);
  lf = user_loop(ls, tl);

  result = 0;

  tx = NULL;
  while((tx = find_task_loop(ls, tl, tx, os->o_type_client)) != NULL){
    lk = send_link_loop(ls, tl, tx, os->o_type_msg);
#ifdef DEBUG
    fprintf(stderr, "broadcast[%p]: relaying inform to %p\n", tl, tx);
#endif
    if(lk){
      if(lk->k_type != os->o_type_msg){
        mm = create_msg();
        lk->k_data = mm;
        lk->k_type = os->o_type_msg;
      } else {
        mm = lk->k_data;
      }
      if(mm){
        lk->k_type = os->o_type_msg;
        line_to_msg(lf->l_line, mm);
      } else {
        result = -1;
      }
    }
  }

  return result;
}

int flush_reply(struct list_loop *ls, struct task_loop *tl)
{
  /* only to be called when terminating, may have sent a message to leaf but not gotten reply yet  */
  struct mul_leaf *lf;
  struct link_loop *lk;
  struct overall_state *os;
  struct mul_msg *mm;

#ifdef DEBUG
  fprintf(stderr, "flush: trying to turn outstanding around messages\n");
#endif

  /* sanity checks happen in run */
  os = global_loop(ls);
  lf = user_loop(ls, tl);

  lk = work_link_loop(ls, tl);
  if(lk != NULL){
    if(lf->l_idle){
#ifdef DEBUG
      fprintf(stderr, "flush: major logic failure: idle, but busy processing request\n");
      abort();
#endif
      return 0; /* we aren't expecting a reply */
    }
  }

  reset_link_loop(ls, tl);
  lf->l_idle = 0; 

  while((lk = receive_link_loop(ls, tl)) != NULL){

    if((lk->k_type != os->o_type_msg) || (lk->k_data == NULL)){
#ifdef DEBUG
      fprintf(stderr, "flush: bad request (type=%d), discarding\n", lk->k_type);
#endif
      discard_link_loop(ls, lk);
    } else {

      mm = lk->k_data;
      if(make_fail_reply_msg(mm) < 0){
#ifdef DEBUG
        fprintf(stderr, "flush: critical problem: unable to honour reply\n");
        /* we may have to kill client connection */
        abort();
#endif
        discard_link_loop(ls, lk);
      }
      respond_link_loop(ls, lk);
    }
  }

  return 0;
}

int timeout_reply(struct list_loop *ls, struct task_loop *tl)
{
  struct mul_leaf *lf;
  struct link_loop *lk;
  struct overall_state *os;
  struct mul_msg *mm;

#ifdef DEBUG
  fprintf(stderr, "timeout: no response, sending timeout\n");
#endif

  /* sanity checks happen in run */
  os = global_loop(ls);
  lf = user_loop(ls, tl);

  if(lf->l_idle){
#ifdef DEBUG
    fprintf(stderr, "timeout: major logic problem: am idle but timing out\n");
    abort();
#endif
    return -1; /* we aren't expecting a reply */
  }

  lk = work_link_loop(ls, tl);
  if(lk == NULL){
#ifdef DEBUG
    fprintf(stderr, "timeout: unable to recover pending request\n");
#endif
    return -1; /* client probably has been terminated */
  }

  if((lk->k_type != os->o_type_msg) || (lk->k_data == NULL)){
#ifdef DEBUG
    fprintf(stderr, "timeout: logic problem: wrong type acknowledged request\n");
    abort();
#endif
    discard_link_loop(ls, lk);
    return -1; /* we kept the wrong message type, internal problem */
  }

  mm = lk->k_data;
  lf->l_idle = 1; /* will turn around this message */

  if(make_fail_reply_msg(mm) < 0){
#ifdef DEBUG
    fprintf(stderr, "timeout: unable to generate fail reply, discarding\n");
    /* TODO: may have to kill other end, otherwise it will hang */
    abort();
#endif
    discard_link_loop(ls, lk);
    return -1; /* unable to say things have failed */
  }

#ifdef DEBUG
  fprintf(stderr, "timeout: have sent timeout reply\n");
#endif

  respond_link_loop(ls, lk);

  return 0;
}

int generate_reply(struct list_loop *ls, struct task_loop *tl)
{
  struct mul_leaf *lf;
  struct link_loop *lk;
  struct overall_state *os;
  struct mul_msg *mm;
  int result;

  /* sanity checks happen in run */
  os = global_loop(ls);
  lf = user_loop(ls, tl);

  if(lf->l_idle){
#ifdef DEBUG
    fprintf(stderr, "generate: nobody interested in this reply, discarding\n");
#endif
    return 0; /* we aren't expecting a reply */
  }

  lk = work_link_loop(ls, tl);
  if(lk == NULL){
#ifdef DEBUG
    fprintf(stderr, "generate: unable to recover message\n");
#endif
    return -1; /* client has probably gone */
  }

  if((lk->k_type != os->o_type_msg) || (lk->k_data == NULL)){
#ifdef DEBUG
    fprintf(stderr, "generate: major logic problem: acknowledged link not matching\n");
    abort();
#endif
    discard_link_loop(ls, lk);
    return -1; /* we kept the wrong message type, internal problem */
  }

  mm = lk->k_data;
  result = line_is_reply_msg(lf->l_line, mm);
  if(result > 0){ /* if reply matches, can send it back */
    result = line_to_msg(lf->l_line, mm);
  } else { /* else mark as failed */
#ifdef DEBUG
    fprintf(stderr, "generate: read line is not response to request in queue\n");
#endif
    result = (-1);
  }

  if(result < 0){ /* not a matching response or unable to generate message */
    if(make_fail_reply_msg(mm) < 0){ /* last ditch attempt */
#ifdef DEBUG
      fprintf(stderr, "generate: unable to queue read response\n");
      /* WARNING: serious problem, we can't honour reply, client will hang */
      abort();
#endif
      discard_link_loop(ls, lk);
      return -1;
    }
  }

  respond_link_loop(ls, lk);
  lf->l_idle = 1; /* turned around a message, go wait for next */

  return 1;
}

int issue_request(struct list_loop *ls, struct task_loop *tl)
{
  struct mul_leaf *lf;
  struct link_loop *lk;
  struct overall_state *os;
  struct mul_msg *mm;

  os = global_loop(ls);
  lf = user_loop(ls, tl);

  lk = receive_link_loop(ls, tl);
  if(lk == NULL){
    return 0;
  }

  if((lk->k_type != os->o_type_msg) || (lk->k_data == NULL)){
    discard_link_loop(ls, lk);
    return 0;
  }

  if(!(lf->l_idle)){
    /* TODO, need to emit a fail response */
#ifdef DEBUG
    fprintf(stderr, "issue: logic problem: not idle, yet issuing more requests\n");
    abort();
#endif
    return -1;
  }

  mm = lk->k_data;
  if(line_from_msg(lf->l_line, mm) < 0){
    if(make_fail_reply_msg(mm) < 0){
#ifdef DEBUG
      fprintf(stderr, "issue: unable to fail incomming request\n");
      /* WARNING: this will lock up the client */
      abort();
#endif
      discard_link_loop(ls, lk);
    } else {
      respond_link_loop(ls, lk);
    }
    return -1;
  } 
  
  set_time_loop(ls, tl, &(mm->m_tv));
  ack_link_loop(ls, tl, lk);

  lf->l_idle = 0;

  return 0;
}

int run_leaf(struct list_loop *ls, struct task_loop *tl, int mask)
{
  struct mul_leaf *lf;
  int fd, result, update;
  char *cmd;
  struct overall_state *os;

  /* WARNING: on shutdown should bounce back all queued requests to senders */

  update = LO_STOP_MASK | LO_READ_MASK;

  os = global_loop(ls);
  lf = user_loop(ls, tl);
  if((lf == NULL) || (os == NULL)){
    stop_loop(ls, tl);
    return 0;
  }

  if(lf->l_fresh){
    fd = fd_loop(ls, tl);
    exchange_katcl(lf->l_line, fd);
    lf->l_fresh = 0;
  }

  if(mask & LO_STOP_MASK){
#if 0
    /* unclear if libloop can handle turnarounds this late */
    flush_reply(ls, tl); 
#endif
    stop_leaf(lf);
    return 0;
  }

  if(mask & LO_TIME_MASK){
    /* TODO might want to timeout all of the other commands in the queue too */
    timeout_reply(ls, tl);
  }

  if(mask & LO_WRITE_MASK){
    if(flushing_katcl(lf->l_line)){
      if(write_katcl(lf->l_line) < 0){
        flush_reply(ls, tl); 
        stop_loop(ls, tl);
        return 0;
      }
    }
  }

  if(mask & LO_READ_MASK){
    result = read_katcl(lf->l_line);
#ifdef DEBUG
    fprintf(stderr, "leaf: read result=%d\n", result);
#endif
    /* TODO: possibly defer stop in order to collect final error messages */
    if(result){
      flush_reply(ls, tl); 
      stop_loop(ls, tl);
      return 0;
    }
  }

  if(mask & LO_WAIT_MASK){
    issue_request(ls, tl);
  }

  while(have_katcl(lf->l_line)){
    cmd = arg_string_katcl(lf->l_line, 0);
    if(cmd){
#ifdef DEBUG
      fprintf(stderr, "leaf: have a line to process, name <%s>\n", cmd);
#endif
      if(arg_reply_katcl(lf->l_line)){
        generate_reply(ls, tl);
      } else if(arg_inform_katcl(lf->l_line)){
        if(!strcmp(cmd, "#version")){
          if(lf->l_version){
            free(lf->l_version);
            lf->l_version = NULL;
          }
          lf->l_version = arg_copy_string_katcl(lf->l_line, 1);
        } else if(!strcmp(cmd, "#log")){
          broadcast_reply(ls, tl);
        }
#ifdef DEBUG
      } else {
        fprintf(stderr, "leaf: unhandled message <%s>\n", cmd);
#endif
      }
    }
  }

  if(lf->l_idle){
    update |= LO_WAIT_MASK;
  } else {
    update |= LO_TIME_MASK;
  }

  if(flushing_katcl(lf->l_line)){
    update |= LO_WRITE_MASK;
  }

  set_mask_loop(ls, tl, update, 0);

  return 0;
}

static void stop_leaf(struct mul_leaf *lf)
{
  if(lf == NULL){
    return;
  }

  if(lf->l_line){
    destroy_katcl(lf->l_line, 0);
    lf->l_line = NULL;
    lf->l_line = NULL;
  }

  if(lf->l_name){
    free(lf->l_name);
    lf->l_name = NULL;
  }

  if(lf->l_version){
    free(lf->l_version);
    lf->l_version = NULL;
  }

  free(lf);
}

struct task_loop *start_leaf(struct list_loop *ls, char *remote)
{
  struct task_loop *tl;
  struct mul_leaf *lf;
  struct timeval tv;
  struct overall_state *os;

  if(remote == NULL){
    return NULL;
  }

  os = global_loop(ls);
  if(os == NULL){
    return NULL;
  }

  lf = malloc(sizeof(struct mul_leaf));
  if(lf == NULL){
    return NULL;
  }

  lf->l_fresh = 1;
  lf->l_line = create_katcl(-1);
  lf->l_name = strdup(remote);
  lf->l_version = NULL;
  lf->l_idle = 1;

  if((lf->l_line == NULL) || (lf->l_name == NULL)){
    stop_leaf(lf);
    return NULL;
  }

  tv.tv_sec = 5;
  tv.tv_usec = 0;

  tl = connect_tcp_joint_loop(ls, remote, lf, NULL, LO_STOP_MASK | LO_READ_MASK | LO_WAIT_MASK, &run_leaf, &tv, os->o_type_leaf);

  return tl;
}
