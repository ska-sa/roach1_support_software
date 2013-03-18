#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sysexits.h>
#include <fcntl.h>
#include <signal.h>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <katcp.h>
#include <netc.h>

#include "raw.h"
#include "config.h"

struct upload_parms
{
  int p_port;
  char *p_name;
  unsigned int p_size;
};

int upload_receive(char *file, unsigned int size, char *name, int port)
{
  int nfd, dfd, lfd, rr, run, wr;
  unsigned int got;
#define BUFFER 1024
  char buffer[BUFFER];


  /* TODO: could create tmpfiles which are renamed on success ... */

  dfd = open(file, O_WRONLY | O_TRUNC | O_CREAT, S_IWUSR);

  if(dfd < 0){
    return -1;
  }

  lfd = net_listen(name, port, 0);
  if(lfd < 0){
    close(dfd);
    return -1;
  }

  nfd = accept(lfd, NULL, 0);
  close(lfd);

  if(nfd < 0){
    close(dfd);
    return -1;
  }

  got = 0;
  for(run = 1; run > 0;){
    rr = read(nfd, buffer, BUFFER);

    if(rr <= 0){
      run = rr;
      break;
    }
    got += rr;

    wr = write(dfd, buffer, rr);
    if(wr < rr){
      run = (-1);
      break;
    }

    alarm(POCO_UPLOAD_TIMEOUT); /* reset interruption */
  }
  
  close(nfd);

  if((size > 0) && (got != size)){
    unlink(name);
    run = (-1);
  } else {
    fchmod(dfd, S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP);
  }

  close(dfd);

  return run;
}

int upload_child(void *data)
{
  struct upload_parms *up;
  int result;

  up = data;
  signal(SIGALRM, SIG_DFL);

  alarm(POCO_UPLOAD_TIMEOUT); /* schedule interruption */
  result = upload_receive(up->p_name, up->p_size, NULL, up->p_port);
  alarm(0); /* don't interrupt */

  return result;

#if 0
  /* the proper way of doing this */
  struct sigaction sag, sog;
  sigset_t nst, ost;
  int result;

  /* set handler */
  sag.sa_handler = &signal_handler;
  sigemptyset(&(sag.sa_mask));
  sag.sa_flags = 0;
  sigaction(SIGALRM, &sag, &sog);

  /* enable interruption */
  sigemptyset(&nst);
  sigaddset(&nst, SIGALRM);
  sigprocmask(SIG_UNBLOCK, &nst, &ost);
  
  alarm(POCO_UPLOAD_TIMEOUT); /* schedule interruption */
  ... 
  do work
  ...
  alarm(0); /* don't interrupt */
  
  /* restore mask */
  sigprocmask(SIG_SETMASK, &ost, NULL);

  /* restore handler */
  sigaction(SIGALRM, &sog, NULL);
#endif
  
}

void upload_collect(struct katcp_dispatch *d, int status)
{
  int code;

  append_string_katcp(d, KATCP_FLAG_STRING | KATCP_FLAG_FIRST, "#uploadbof");

  if(WIFEXITED(status)){
    code = WEXITSTATUS(status);
    if(code > 0){
      append_string_katcp(d, KATCP_FLAG_STRING, KATCP_FAIL);
      append_string_katcp(d, KATCP_FLAG_STRING | KATCP_FLAG_LAST, "abnormal exit");
    } else {
      append_string_katcp(d, KATCP_FLAG_STRING | KATCP_FLAG_LAST, KATCP_OK);
    }
  } else {
    append_string_katcp(d, KATCP_FLAG_STRING, KATCP_FAIL);
    append_string_katcp(d, KATCP_FLAG_STRING | KATCP_FLAG_LAST, "abnormal termination");
  }
}

int uploadbof_cmd(struct katcp_dispatch *d, int argc)
{
  struct state_raw *sr;
  char *name;
  int len, result;
  struct upload_parms upload, *up;

  up = &upload;
  
  up->p_name = NULL;
  up->p_port = 0;
  up->p_size = 0;

  sr = get_current_mode_katcp(d);
  if(sr == NULL){
    log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "unable to get raw state");
    return KATCP_RESULT_FAIL;
  }

  if(argc <= 2){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "need a port and file name to save data");
    return KATCP_RESULT_INVALID;
  }

  up->p_port = arg_unsigned_long_katcp(d, 1);
  if((up->p_port <= 1024) || (up->p_port > 0xffff)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "port %d not in valid range", up->p_port);
    return KATCP_RESULT_INVALID;
  }

  name = arg_string_katcp(d, 2);
  if(name == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to acquire first parameter");
    return KATCP_RESULT_FAIL;
  }

  if(argc > 2){
    up->p_size = arg_unsigned_long_katcp(d, 3);
  }

  if(strchr(name, '/') || (name[0] == '.')){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "refusing to upload file containing path information");
    return KATCP_RESULT_FAIL;
  }

  len = strlen(name) + strlen(sr->s_bof_path) + 1;
  up->p_name = malloc(len + 1);
  if(up->p_name == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to allocate %d bytes", len + 1);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "allocation");
    return KATCP_RESULT_OWN;
  }

  result = snprintf(up->p_name, len + 1, "%s/%s", sr->s_bof_path, name);
  if(result != len){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "major logic failure: expected %d from snprintf, got %d", len, result);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "internal");
    free(up->p_name);
    return KATCP_RESULT_OWN;
  }

  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "receiving data on port %d for file %s", up->p_port, up->p_name);

  if(spawn_child_katcp(d, NULL, &upload_child, up, &upload_collect) <= 0){
    log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "unable to initiate collection process");
    return KATCP_RESULT_FAIL;
  }

  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "initiating reception of file %s", name);
  free(up->p_name);

  return KATCP_RESULT_OK;
}

