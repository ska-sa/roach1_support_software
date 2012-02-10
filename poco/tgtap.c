#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <katcp.h>

#include "core.h"
#include "modes.h"
#include "poco.h"
#include "misc.h"

#define BUFFER_SIZE 16 

static void stale_tgtap_watch(struct katcp_dispatch *d, int status)
{
  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "prior tap instance exited with status 0x%x", status);
}

static void current_tgtap_watch(struct katcp_dispatch *d, int status)
{
  if(WIFEXITED(status)){
    if(WEXITSTATUS(status)){
      log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "tgtap exited with code %d", WEXITSTATUS(status));
    } else {
      log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "tgtap exited normally");
    }
  } else if(WIFSIGNALED(status)){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "tgtap killed by signal %d", WTERMSIG(status));
  } else {
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "tgtap exited with status 0x%x", status);
  }
}

static int start_tap(struct katcp_dispatch *d, char *name, char *ip, char *port, char *mac)
{
  pid_t pid;
  int fd, fi, result, i;
  char *vector[10];
  struct poco_core_entry *pce;
  char *full_name;

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "attempting to terminate process matching %s", name);
  result = end_shared_katcp(d, name, 0, NULL);

  if(result < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to terminate any prior instance of %s", name);
    return -1;
  }

  if(result > 0){
    if(watch_shared_katcp(d, name, 0, &stale_tgtap_watch) < 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to change monitoring for tgtap");
      return -1;
    }
  }

  pce = by_name_pce(d, name);
  if(pce == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "register %s seems unavailable", name);
    return -1;
  }

  full_name = full_name_pce(pce);
  if(full_name == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "internal failure, no full name available for %s", name);
    return -1;
  }

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "about to spawn subprocess for %s", full_name);

  pid = fork();
  if(pid != 0){
    if(pid < 0){
      return -1;
    }

    if(watch_shared_katcp(d, name, pid, &current_tgtap_watch) < 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to monitor tgtap subprocess");
      return -1;
    }

    log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "subprocess for %s is %d", full_name, pid);

    return 0;
  }

#define FD_RANGE 256
  for(fi = 0; fi < FD_RANGE; fi++){
    close(fi);
  }
#undef FD_RANGE

  fd = open("/dev/null", O_RDWR);
  if(fd >= 0){
    for(fi = 0; fi <= STDERR_FILENO; fi++){
      if(fd != fi){
        dup2(fd, fi);
      }
    }
    if(fd > STDERR_FILENO){
      close(fd);
    }
  }

  i = 0;

  vector[i++] = "tgtap";
  vector[i++] = "-b";
  vector[i++] = full_name;

  vector[i++] = "-a";
  vector[i++] = ip;

  vector[i++] = "-t";
  vector[i++] = name;

  if(mac){
    vector[i++] = "-m";
    vector[i++] = mac;
  }

  if(port){
    vector[i++] = "-p";
    vector[i++] = port;
  }

  vector[i++] = NULL;

  execvp(vector[0], vector);

  exit(1);

  return 0;
}

int stop_tgtap_cmd(struct katcp_dispatch *d, int argc)
{
  char *name;
  int result;

  if(argc <= 1){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "need a register name");
    return KATCP_RESULT_INVALID;
  }

  name = arg_string_katcp(d, 1);
  if(name == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "internal failure while acquireing parameters");
    return KATCP_RESULT_FAIL;
  }

  result = end_shared_katcp(d, name, 0, NULL);
  if(result < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to terminate any prior instance of %s", name);
    return KATCP_RESULT_FAIL;
  } 
  
  if(result == 0){
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "could not find an instance of %s to terminate", name);
  }

  return KATCP_RESULT_OK;
}

int start_tgtap_cmd(struct katcp_dispatch *d, int argc)
{
  char *name, *ip, *port, *mac;

  mac = NULL;
  port = NULL;

  if(argc <= 3){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "need at least a register name, a mac address and an ip address");
    return KATCP_RESULT_INVALID;
  }

  name = arg_string_katcp(d, 1);
  mac = arg_string_katcp(d, 2);
  ip = arg_string_katcp(d, 3);

  if((ip == NULL) || (name == NULL) || (mac == NULL)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to acquire parameters");
    return KATCP_RESULT_INVALID;
  }

  if(argc > 4){
    port = arg_string_katcp(d, 4);
    if(port == NULL){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to acquire optional port");
      return KATCP_RESULT_INVALID;
    }
  }

  if(start_tap(d, name, ip, port, mac) < 0){
    return KATCP_RESULT_FAIL;
  }

  return KATCP_RESULT_OK;
}
