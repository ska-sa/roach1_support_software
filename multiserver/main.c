#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <loop.h>
#include <katcp.h>

#include "multiserver.h"

void usage(char *app)
{
  printf("usage: %s -h -p port\n", app);
  printf("-h                   this help\n");
  printf("-p [address:]port    interface to bind\n");
}

struct overall_state *setup_overall(void)
{
  struct overall_state *os;

  os = malloc(sizeof(struct overall_state));
  if(os == NULL){
    return NULL;
  }

  os->o_type_client = 0;
  os->o_type_inter = 0;
  os->o_type_leaf = 0;
  os->o_type_msg = 0;

  return os;
}

int claims_overall(struct list_loop *ls, struct overall_state *os)
{

  os->o_type_client = claim_task_name_loop(ls, CLIENT_TASK_LIMIT, 2, CLIENT_NAME);
  if(os->o_type_client <= 0){
    return -1;
  }

  os->o_type_inter = claim_task_name_loop(ls, INTER_TASK_LIMIT, 2, INTER_NAME);
  if(os->o_type_inter <= 0){
    return -1;
  }

  os->o_type_leaf = claim_task_name_loop(ls, LEAF_TASK_LIMIT, 2, LEAF_NAME);
  if(os->o_type_leaf <= 0){
    return -1;
  }

  os->o_type_msg = claim_link_name_loop(ls, MSG_TASK_LIMIT, 2, MSG_NAME, &release_msg);
  if(os->o_type_msg <= 0){
    return -1;
  }

  /* TODO: check return code */

  return 0;
}

void shutdown_overall(struct overall_state *os)
{
  if(os == NULL){
    return;
  }

  free(os);
}

/****************************************************************/

int main(int argc, char **argv)
{
  struct list_loop *ls;
  struct overall_state *os;
  struct katcp_dispatch *kd;

  int i, j, c;
  int fd, result;
  char *app, *data;
  int mon_flags;

  char *log_file = NULL;

  char *user_loop = NULL;
  char *dir_root = NULL;
  int enable_fifo = 0;
  int console_flags = LO_RUN_CONSOLE;
  int enable_crash = 0;
  int enable_debug = 0;

#if 0
  int task_limit = 32;
  int link_limit = 64;
#endif
  int verbose = 1;
  char *port = "7147";

  /* arg parse */

  app = argv[0];
  i = j = 1;

  while (i < argc) {
    if (argv[i][0] == '-') {
      c = argv[i][j];
      switch (c) {
        case 'F' :
          enable_fifo = 1;
          j++;
          break;
        case 'h' :
          usage(app);
          error_exit(EX_OK, NULL);
          break;
        case 'v' :
          verbose++;
          j++;
          break;
        case 'q' :
          verbose = 0;
          j++;
          break;
        case 'D' :
        case 'C' :
        case 'p' :
          j++;
          data = NULL;
          if (argv[i][j] == '\0') {
            if(((i + 1) < argc) && (argv[i + 1][0] != '-')){
              j = 0;
              i++;
              data = argv[i] + j;
            } else {
              data = NULL;
            }
          } else {
            data = argv[i] + j;
          }
          switch(c){
            case 'D' :
              enable_debug = 1;
              log_file = data;
              /* fall ? */
            case 'C' :
              enable_crash = 1;
              log_file = data;
              break;
            case 'p' :
              port = data;
              break;
          }
          i++;
          j = 1;
          break;
        case '-' :
          j++;
          break;
        case '\0':
          j = 1;
          i++;
          break;
        default:
          error_exit(EX_USAGE, "%s: unknown option -%c\n", app, argv[i][j]);
          break;
      }
    } else {
      error_exit(EX_USAGE, "%s: unknown parameter %s\n", app, argv[i]);
      i++;
    }
  }

  if(enable_crash || enable_debug){
    if(log_file == NULL){
      /* no detach if output requested to stderr */
      console_flags |= (~LO_DETACH_CONSOLE);
    }
  }

  mon_flags = LO_MON_START | LO_MON_DEFER; /* start output, but write to stderr until detach or absolve */
  if(enable_crash){
    mon_flags |= LO_MON_ABORT;
    if(!enable_debug){
      mon_flags |= LO_MON_TRUNCATE;
    }
  }
  if(enable_debug){
    mon_flags |= LO_MON_EXIT;
  }
  if(enable_fifo){
    mon_flags |= LO_MON_FIFO;
  }

  if(error_monitor(log_file, mon_flags)){
    error_exit(EX_OSERR, "%s: unable to start error monitor\n", app);
  }

  if(verbose){
    fprintf(stderr, "%s: starting\n", app);
  }

  /* aim: keep (possible) parent from exiting if logging to */
  fd = start_profile_console(NULL, console_flags);
  if(fd < 0){
    error_exit(EX_OSERR, "%s: unable to perform setup\n", app);
  }

  os = setup_overall();
  if(os == NULL){
    error_exit(EX_OSERR, "%s: unable to set up global state\n", app);
  }

  ls = startup_loop(os, GLOBAL_TASK_LIMIT, GLOBAL_LINK_LIMIT);
  if(ls == NULL){
    error_exit(EX_SOFTWARE, "%s: unable to setup loop\n", app);
  }

  if(claims_overall(ls, os)){
    error_exit(EX_SOFTWARE, "%s: unable to claim types\n", app);
  }

  if(start_resolver_loop(ls, NULL, 8, 60) == NULL){
    error_exit(EX_SOFTWARE, "%s: unable to setup resolver\n", app);
  }
  
  kd = startup_katcp();
  if(kd == NULL){
    error_exit(EX_SOFTWARE, "%s: unable to setup client template\n", app);
  }
  version_katcp(kd, SUBSYSTEM, MAJOR, MINOR);
  result = 0;
  result += register_katcp(kd, "?busy", "deliberate per client busy hang", &busy_cmd);
  result += register_katcp(kd, "?leaf-connect", "connect to a subordinate system", &leaf_connect_cmd);
  result += register_katcp(kd, "?leaf-list", "display connections to subordinate systems", &leaf_list_cmd);
  result += register_katcp(kd, "?leaf-relay", "send a command to a given child type", &leaf_relay_cmd);
  result += register_katcp(kd, "?x-test", "test of the xlookup logic", &x_test_cmd);
  if(result){
    error_exit(EX_SOFTWARE, "%s: unable to enroll commands\n", app);
  }

  if(add_tcp_joint_listen_loop(ls, port, kd, &setup_client, LO_STOP_MASK | LO_WAIT_MASK | LO_READ_MASK | LO_WRITE_MASK, &run_client, NULL, os->o_type_client) == NULL){
    error_exit(EX_SOFTWARE, "%s: unable to generate client listener on %s\n", app, port);
  }

  if(verbose){
    fprintf(stderr, "%s: listening for clients on %s\n", app, port);
  }

  close(fd);

#ifndef DEBUG
  if(misc_drop_loop(user_loop, dir_root, 1)){
    fprintf(stderr, "%s: unable to reduce privileges\n", app);
    return EX_SOFTWARE;
  }
#endif

  if(enable_crash || enable_debug){
    if(!enable_debug){ /* for crash only emit things when they go wrong */
      error_disable();
    }
    if(log_file){ /* if there is a file we can detach */
      error_detach();
    }
  } else {
    error_absolve(); /* no logging requested, so stop child and thus detach */
  }

  /* WARNING: after this point only DEBUG to stderr */

  main_loop(ls);

  shutdown_loop(ls);

#if 0
  stop_state(mg);
#endif
  shutdown_overall(os);

  error_absolve();

  return EX_OK;
}
