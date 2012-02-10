
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sched.h>
#include <errno.h>

#include <sys/wait.h>
#include <sys/types.h>

#include <unistd.h>

#include "katcp.h"
#include "katpriv.h"
#include "katsensor.h"
#include "netc.h"

#define SHARED_MAGIC 0x548a52ed

#ifdef DEBUG
static void sane_shared_katcp(struct katcp_dispatch *d)
{
  struct katcp_shared *s;

  if(d == NULL){
    fprintf(stderr, "shared sane: invalid handle\n");
    abort();
  }

  if(d->d_shared == NULL){
    fprintf(stderr, "shared sane: no shared component in handle\n");
    abort();
  }

  s = d->d_shared;

  if(s->s_magic != SHARED_MAGIC){
    fprintf(stderr, "shared sane: bad magic 0x%08x, expected 0x%x\n", s->s_magic, SHARED_MAGIC);
    abort();
  }

  if(s->s_mode >= s->s_size){
    fprintf(stderr, "shared sane: mode=%u over size=%u\n", s->s_mode, s->s_size);
    abort();
  }

  if(s->s_template == d){
    if(d->d_clone >= 0){
      fprintf(stderr, "shared sane: instance %p is template but also clone id %d\n", d, d->d_clone);
      abort();
    }
  } else {
    if((d->d_clone < 0) || (d->d_clone >= s->s_count)){
      fprintf(stderr, "shared sane: instance %p is not template but clone id %d out of range 0:%d\n", d, d->d_clone, s->s_count);
      abort();
    }
  }

  if(s->s_used > s->s_count){
    fprintf(stderr, "shared sane: used %d more than have %d\n", s->s_used, s->s_count);
    abort();
  }

}
#else
#define sane_shared_katcp(d)
#endif

int startup_shared_katcp(struct katcp_dispatch *d)
{
  struct katcp_shared *s;

  s = malloc(sizeof(struct katcp_shared));
  if(s == NULL){
    return -1;
  }

  s->s_magic = SHARED_MAGIC;

  s->s_vector = NULL;

  s->s_size = 0;
  s->s_modal = 0;

  s->s_commands = NULL;
  s->s_mode = 0;

  s->s_template = NULL;
  s->s_clients = NULL;

  s->s_count = 0;
  s->s_used = 0;

  s->s_lfd = (-1);

  s->s_table = NULL;
  s->s_entries = 0;

  s->s_queue = NULL;
  s->s_length = 0;

  s->s_build_state = NULL;
  s->s_version_subsystem = NULL;

  s->s_version_major = 0;
  s->s_version_minor = 0;

  s->s_sensors = NULL;
  s->s_tally = 0;

  s->s_vector = malloc(sizeof(struct katcp_entry));
  if(s->s_vector == NULL){
    free(s);
    return -1;
  }

  s->s_vector[0].e_name = NULL;
  s->s_vector[0].e_enter = NULL;
  s->s_vector[0].e_leave = NULL;
  s->s_vector[0].e_state = NULL;
  s->s_vector[0].e_clear = NULL;

  s->s_size = 1;

#ifdef DEBUG
  if(d->d_shared){
    fprintf(stderr, "startup shared: major logic failure: instance %p already has shared data %p\n", d, d->d_shared);
    abort();
  }
#endif

  s->s_template = d;
  d->d_shared = s;

  return 0;
}

void shutdown_shared_katcp(struct katcp_dispatch *d)
{
  int i;
  struct katcp_shared *s;
  struct katcp_cmd *c;

  if(d == NULL){
    return;
  }

  s = d->d_shared;
  if(s == NULL){
#ifdef DEBUG
    fprintf(stderr, "shutdown: warning, no shared state for %p\n", d);
#endif
    return;
  }

  /* clear modes only once, when we clear the template */
  if(d->d_clone < 0){
    for(i = 0; i < s->s_size; i++){
      s->s_mode = i; /* be nice to the clear function, get current mode works */
      if(s->s_vector[i].e_clear){
        (*(s->s_vector[i].e_clear))(d);
      }
      s->s_vector[i].e_state = NULL;
    }
  }

  if(d->d_clone >= 0){
#ifdef DEBUG
    fprintf(stderr, "shutdown: releasing clone %d/%d\n", d->d_clone, s->s_count);
    if((d->d_clone < 0) || (d->d_clone >= s->s_count)){
      fprintf(stderr, "shutdown: major logic problem: clone=%d, count=%d\n", d->d_clone, s->s_count);
      abort();
    }
    if(s->s_clients[d->d_clone] != d){
      fprintf(stderr, "shutdown: no client match for %p at %d\n", d, d->d_clone);
      abort();
    }
#endif
    s->s_clients[d->d_clone] = NULL;
    s->s_count--;
    if(d->d_clone < s->s_count){
      s->s_clients[d->d_clone] = s->s_clients[s->s_count];
      s->s_clients[d->d_clone]->d_clone = d->d_clone;
    }
    if(s->s_used > s->s_count){
      s->s_used = s->s_count;
    }
    /* if we are a clone, just de-register - don't destroy shared */
    return;
  }

#ifdef DEBUG
  if(s->s_template != d){
    fprintf(stderr, "shutdown: clone=%d, but %p not registered as template (%p instead)\n", d->d_clone, d, s->s_template);
    abort();
  }
#endif

  destroy_sensors_katcp(d);

  while(s->s_count > 0){
#ifdef DEBUG
    if(s->s_clients[0]->d_clone != 0){
      fprintf(stderr, "shutdown: major logic failure: first client is %d\n", s->s_clients[0]->d_clone);
    }
#endif
    shutdown_katcp(s->s_clients[0]);
  }

  /* at this point it is unsafe to call API functions on the shared structure */

  free(s->s_clients);
  s->s_clients = NULL;
  s->s_template = NULL;

  while(s->s_commands != NULL){
    c = s->s_commands;
    s->s_commands = c->c_next;
    c->c_next = NULL;
    shutdown_cmd_katcp(c);
  }

  s->s_mode = 0;

  if(s->s_vector){
    free(s->s_vector);
    s->s_vector = NULL;
  }
  s->s_size = 0;

  if(s->s_table){
    free(s->s_table);
    s->s_table = NULL;
  }
  s->s_entries = 0;

  if(s->s_build_state){
    free(s->s_build_state);
    s->s_build_state = NULL;
  }
  if(s->s_version_subsystem){
    free(s->s_version_subsystem);
    s->s_version_subsystem = NULL;
  }

#ifdef DEBUG
  if(s->s_length > 0){
    fprintf(stderr, "shutdown: probably bad form to rely on shutdown to clear schedule\n");
  }
#endif
  empty_timers_katcp(d);

  free(s);
}

/***********************************************************************/

int link_shared_katcp(struct katcp_dispatch *d, struct katcp_dispatch *cd)
{
  struct katcp_shared *s;
  struct katcp_dispatch **dt;

  s = cd->d_shared;

#ifdef DEBUG
  if(d->d_shared){
    fprintf(stderr, "clone: logic error: clone %p already has shared state\n", d);
    abort();
  }
  if(d->d_clone >= 0){
    fprintf(stderr, "clone: clone already has id %d\n", d->d_clone);
    abort();
  }
  if(s == NULL){
    fprintf(stderr, "clone: logic error: template %p has no shared state\n", cd);
    abort();
  }
  if(s->s_template != cd){
    fprintf(stderr, "clone: logic problem: not cloning from template\n");
    abort();
  }
#endif

  dt = realloc(s->s_clients, sizeof(struct katcp_dispatch) * (s->s_count + 1));
  if(dt == NULL){
    shutdown_katcp(d);
    return -1;
  }
  s->s_clients = dt;
  s->s_clients[s->s_count] = d;

  d->d_clone = s->s_count;
  s->s_count++;

  d->d_shared = s;

  return 0;
}

pid_t pid_by_name_katcp(struct katcp_dispatch *d, char *name)
{
  struct katcp_shared *s;
  int i;

  if(d == NULL){
    return -1;
  }

  s = d->d_shared;
  if(s == NULL){
    return -1;
  }

  if(name == NULL){
    return -1;
  }

  for(i = 0; i < s->s_entries; i++){
    if(!strcmp(s->s_table[i].p_name, name)){
      return s->s_table[i].p_pid;
    }
    i++;
  }

  return 0;
}

#if 0
int check_exit_katcp(struct katcp_dispatch *d, char *name, pid_t pid, struct timeval *tv)
{
  pid_t got;
  int status, result;

#if 1
  if(tv){ /* HACK WARNING - should have a pselect, to be interrupted by sigchild */
    select(0, NULL, NULL, NULL, tv);
  }
#endif

  if(pid <= 0){
    if(name == NULL){
      return -1;
    }
    pid = pid_by_name_katcp(d, name);
    if(pid == 0){
      log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "no records for process %u", pid);
      return -1;
    }
  }

  while((got = waitpid(WAIT_ANY, &status, WNOHANG)) > 0){
    if(got == pid){
      result = 1;
    }
    reap_shared_katcp(d, got, status);
  }

  return result;
}
#endif

#if 0
#define POST_KILL_DELAY 2000
#endif

int end_shared_katcp(struct katcp_dispatch *d, char *name, pid_t pid, struct timeval *tv)
{
  struct katcp_shared *s;
  int i, result, status;
  pid_t got;

  if(d == NULL){
    return -1;
  }

  s = d->d_shared;
  if(s == NULL){
    return -1;
  }

  if(pid <= 0){
    if(name == NULL){
      return -1;
    }

    pid = pid_by_name_katcp(d, name);

    if(pid <= 0){
      return 0;
    }
  }

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "attempting to terminate child process %u", pid);

  if(kill(pid, SIGTERM) < 0){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "unable to kill process %u: %s", s->s_table[i].p_pid, strerror(errno));
    return -1;
  }

  if(tv){
    result = 0;

#if 1
    /* HACK WARNING - we should use pselect with signal handler */
    select(0, NULL, NULL, NULL, tv);
#endif

    while((got = waitpid(WAIT_ANY, &status, WNOHANG)) > 0){
      if(got == pid){
        result = 1;
      }
      reap_shared_katcp(d, got, status);
    }
  } else {
    result = 1;
  }

  return result;
}

int watch_shared_katcp(struct katcp_dispatch *d, char *name, pid_t pid, void (*call)(struct katcp_dispatch *d, int status))
{
  struct katcp_process *pt;
  struct katcp_shared *s;
  int i;

  s = d->d_shared;
  if(s == NULL){
    return -1;
  }

  if((pid <= 0) && (name == NULL)){
    return -1;
  }

  for(i = 0; i < s->s_entries; i++){
    if(((pid > 0) && (s->s_table[i].p_pid == pid)) || (name && !strcmp(s->s_table[i].p_name, name))){
      if(call){ /* overwrite */
        s->s_table[i].p_call = call; 
      } else { /* delete */
        s->s_entries--;
        if(s->s_table[i].p_name){
          free(s->s_table[i].p_name);
        }
        if(i < s->s_entries){
          s->s_table[i].p_call = s->s_table[s->s_entries].p_call;
          s->s_table[i].p_pid  = s->s_table[s->s_entries].p_pid;
          s->s_table[i].p_name = s->s_table[s->s_entries].p_name;
        }
      }
      return 0;
    } 
  }
  
  if(call == NULL){
#ifdef DEBUG
    fprintf(stderr, "watch: attemping to delete nonexistant entry\n");
#endif
    return -1;
  }

  /* new, insert */

  pt = realloc(s->s_table, (s->s_entries + 1) * sizeof(struct katcp_process));
  if(pt == NULL){
    return -1;
  }

  s->s_table = pt;

  s->s_table[s->s_entries].p_pid = pid;
  s->s_table[s->s_entries].p_call = call;
  s->s_table[s->s_entries].p_name = strdup(name);

  if(s->s_table[s->s_entries].p_name == NULL){
    return -1;
  }

  s->s_entries++;

  return 0;
}

int reap_shared_katcp(struct katcp_dispatch *d, pid_t pid, int status)
{
  struct katcp_dispatch *dt;
  struct katcp_shared *s;
  int i;
  void (*call)(struct katcp_dispatch *d, int status);

  s = d->d_shared;
  if(s == NULL){
    return -1;
  }

  /* WARNING - template dispatch doesn't have any valid output */
#if 0
  dt = s->s_template;
  if(dt == NULL){
    return -1;
  } 
#else
  dt = d;
#endif

  for(i = 0; i < s->s_entries; i++){
    if(s->s_table[i].p_pid == pid){
      call = s->s_table[i].p_call;
      s->s_entries--;

      if(s->s_table[i].p_name){
        free(s->s_table[i].p_name);
        s->s_table[i].p_name = NULL;
      }

      if(i < s->s_entries){
        s->s_table[i].p_call = s->s_table[s->s_entries].p_call;
        s->s_table[i].p_pid  = s->s_table[s->s_entries].p_pid;
        s->s_table[i].p_name = s->s_table[s->s_entries].p_name;
      }

      (*call)(dt, status);

      return 0;
    }
  }

  log_message_katcp(dt, KATCP_LEVEL_WARN, NULL, "unwatched child process %d has exited", pid);

  return 0;
}

int listen_shared_katcp(struct katcp_dispatch *d, int count, char *host, int port)
{
  int i;
  struct katcp_shared *s;

  sane_shared_katcp(d);

  s = d->d_shared;
  if(s == NULL){
    return -1;
  }

  s->s_lfd = net_listen(host, port, 0);
  if(s->s_lfd < 0){
    return -1;
  }

  fcntl(s->s_lfd, F_SETFD, FD_CLOEXEC);

  if(s->s_clients){
#ifdef DEBUG
    fprintf(stderr, "listen: unwilling to overwrite %d existing clients\n", s->s_count);
    abort();
#endif
    return -1;
  }

  for(i = 0; i < count; i++){
    if(clone_katcp(d) == NULL){
      if(i == 0){
        close(s->s_lfd);
        s->s_lfd = (-1);
        return -1;
      } else {
#ifdef DEBUG
        fprintf(stderr, "listen: wanted to create %d instances, only could do %d\n", count, i);
#endif
        return i;
      }
    }
  }

#ifdef DEBUG
  fprintf(stderr, "listen: created %d requested instances\n", count);
#endif

  return count;
}

/***********************************************************************/

int mode_cmd_katcp(struct katcp_dispatch *d, int argc)
{
  char *name, *actual;

  sane_shared_katcp(d);

  name = NULL;

  if(d->d_shared->s_modal == 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "logic problem, mode command invoked without modes registered");
    return KATCP_RESULT_FAIL;
  }

  if(argc > 1){
    name = arg_string_katcp(d, 1);
    if(name == NULL){
      return KATCP_RESULT_FAIL;
    }

    if(enter_name_mode_katcp(d, name) < 0){
      return KATCP_RESULT_FAIL;
    }
  }

  actual = query_mode_name_katcp(d);
  if(actual == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "no name available for current mode");
    return KATCP_RESULT_FAIL;
  }

  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "current mode now %s", actual);

  if(name && strcmp(actual, name)){
    extra_response_katcp(d, KATCP_RESULT_FAIL, actual);
  } else {
    extra_response_katcp(d, KATCP_RESULT_OK, actual);
  }

  return KATCP_RESULT_OWN;
}

/***********************************************************************/

int store_mode_katcp(struct katcp_dispatch *d, unsigned int mode, void *state)
{
  return store_full_mode_katcp(d, mode, NULL, NULL, NULL, state, NULL);
}

int store_clear_mode_katcp(struct katcp_dispatch *d, unsigned int mode, void *state, void (*clear)(struct katcp_dispatch *d))
{
  return store_full_mode_katcp(d, mode, NULL, NULL, NULL, state, clear);
}

int store_full_mode_katcp(struct katcp_dispatch *d, unsigned int mode, char *name, int (*enter)(struct katcp_dispatch *d, unsigned int from), void (*leave)(struct katcp_dispatch *d, unsigned int to), void *state, void (*clear)(struct katcp_dispatch *d))
{
  struct katcp_shared *s;
  struct katcp_entry *e;
  char *copy;
  int i, result;

  sane_shared_katcp(d);

  s = d->d_shared;

  if(mode >= s->s_size){ /* expand if needed */
    e = realloc(s->s_vector, sizeof(struct katcp_entry) * (mode + 1));
    if(e == NULL){
      return -1;
    }
    s->s_vector = e;

    for(i = s->s_size; i <= mode; i++){
      s->s_vector[i].e_name = NULL;
      s->s_vector[i].e_enter = NULL;
      s->s_vector[i].e_leave = NULL;
      s->s_vector[i].e_state = NULL;
      s->s_vector[i].e_clear = NULL;
    }

    s->s_size = mode + 1;
  }

  if(name){
    copy = strdup(name);
    if(copy == NULL){
      return -1;
    }
  } else {
    copy = NULL;
  }

  result = 0;

  if(s->s_vector[mode].e_clear){
    (*(s->s_vector[mode].e_clear))(d);
    result = 1;
  }
  if(s->s_vector[mode].e_name != NULL){
    free(s->s_vector[mode].e_name);
    s->s_vector[mode].e_name = NULL;
    result = 1;
  }

  s->s_vector[mode].e_name = copy;
  s->s_vector[mode].e_enter = enter;
  s->s_vector[mode].e_leave = leave;
  s->s_vector[mode].e_state = state;
  s->s_vector[mode].e_clear = clear;

  if(name){
    if(s->s_modal == 0){
      if(register_katcp(d, "?mode", "mode change command (?mode [new-mode])", &mode_cmd_katcp)){
        return -1;
      }
      s->s_modal = 1;
    }
  }

  return result;
}

int is_mode_katcp(struct katcp_dispatch *d, unsigned int mode)
{
  struct katcp_shared *s;

  sane_shared_katcp(d);

  s = d->d_shared;

  return (s->s_mode == mode) ? 1 : 0;
}

void *get_mode_katcp(struct katcp_dispatch *d, unsigned int mode)
{
  sane_shared_katcp(d);

  if(mode >= d->d_shared->s_size){
    return NULL;
  }

  return d->d_shared->s_vector[mode].e_state;
}

void *need_current_mode_katcp(struct katcp_dispatch *d, unsigned int mode)
{
  sane_shared_katcp(d);

  if(mode >= d->d_shared->s_size){
    return NULL;
  }

  if(d->d_shared->s_mode != mode){
    return NULL;
  }

  return d->d_shared->s_vector[mode].e_state;
}

void *get_current_mode_katcp(struct katcp_dispatch *d)
{
  struct katcp_shared *s;

  sane_shared_katcp(d);

  s = d->d_shared;

  return get_mode_katcp(d, s->s_mode);
}

int enter_name_mode_katcp(struct katcp_dispatch *d, char *name)
{
  struct katcp_shared *s;
  unsigned int i;

  sane_shared_katcp(d);

  s = d->d_shared;

  for(i = 0; i < s->s_size; i++){
    if(s->s_vector[i].e_name && !(strcmp(s->s_vector[i].e_name, name))){
      return enter_mode_katcp(d, i);
    }
  }

  log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unknown mode %s", name);
  return -1;
}

int enter_mode_katcp(struct katcp_dispatch *d, unsigned int mode)
{
  struct katcp_shared *s;
  unsigned int to, from;

  sane_shared_katcp(d);

  s = d->d_shared;

  if(mode >= s->s_size){
    /* TODO: report errors */
    return -1;
  }

  from = s->s_mode;
  if(from == mode){
    /* TODO: report no change */
    return s->s_mode;
  }
  to = mode;

  if(s->s_vector[from].e_leave){
    (*(s->s_vector[from].e_leave))(d, to);
  }

  s->s_mode = to;

  if(s->s_vector[to].e_enter){
    s->s_mode = (*(s->s_vector[to].e_enter))(d, from);
  }

  if(s->s_mode >= s->s_size){
    s->s_mode = 0;
  }

  if(from != s->s_mode){ /* mode has changed,  report it */
    if(s->s_vector[s->s_mode].e_name){ /* but only if it has a name */
      broadcast_inform_katcp(d, "#mode", s->s_vector[s->s_mode].e_name);
    }
  }

  return s->s_mode;
}

int query_mode_katcp(struct katcp_dispatch *d)
{
  struct katcp_shared *s;

  sane_shared_katcp(d);

  s = d->d_shared;

  return s->s_mode;
}

char *query_mode_name_katcp(struct katcp_dispatch *d)
{
  struct katcp_shared *s;

  sane_shared_katcp(d);

  s = d->d_shared;

  return s->s_vector[s->s_mode].e_name;
}
