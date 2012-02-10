#include <stdio.h>

#include <katcp.h>

#include "multiserver.h"

int busy_cmd(struct katcp_dispatch *d, int argc)
{
  send_katcp(d, KATCP_FLAG_FIRST | KATCP_FLAG_LAST | KATCP_FLAG_STRING, "#busy");

  return KATCP_RESULT_RESUME;
}

static int leaf_relay_collect(struct katcp_dispatch *d, int argc);

int leaf_relay_cmd(struct katcp_dispatch *d, int argc)
{
  struct mul_msg *mm;
  char *ptr;
  int i, flags;

  if(argc < 3){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "need a remote name and command");
    return KATCP_RESULT_FAIL;
  }

  ptr = arg_string_katcp(d, 1);
  if(ptr == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to retrieve remote name");
    return KATCP_RESULT_FAIL;
  }

#ifdef DEBUG
  fprintf(stderr, "relay: about to issue command to <%s>\n", ptr);
#endif

  mm = issue_katmc(d, ptr);
  if(mm == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to issue a request");
    return KATCP_RESULT_FAIL;
  }

  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "issuing request");

  flags = KATCP_FLAG_FIRST;
  for(i = 2 ; i < argc; i++){
    /* WARNING: this ignores binary data, will have to fix for real uses */
    ptr = arg_string_katcp(d, i);
    if(ptr){
      append_string_msg(mm, flags, ptr);
    }
    flags = 0;
  }

  continue_katcp(d, 0, &leaf_relay_collect);
  return KATCP_RESULT_RESUME;
}

static int leaf_relay_collect(struct katcp_dispatch *d, int argc)
{
  struct mul_msg *mm;
  char *status;

  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "collecting request");

  mm = collect_katmc(d);
  if(mm == NULL){
    log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "nothing to collect");
    return KATCP_RESULT_FAIL;
  }

  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "got something to collect");

  status = arg_string_msg(mm, 1);
  if(status == NULL){
    log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "unable to acquire status");
    release_katmc(d, mm);
    return KATCP_RESULT_FAIL;
  }

  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "got status %s, %d requests outstanding", status, outstanding_katmc(d));

  send_katcp(d, 
      KATCP_FLAG_FIRST | KATCP_FLAG_STRING, "!leaf-relay", 
      KATCP_FLAG_LAST  | KATCP_FLAG_STRING, status);

  release_katmc(d, mm);
  return KATCP_RESULT_OWN;
}

int leaf_connect_cmd(struct katcp_dispatch *d, int argc)
{
  char *remote;
  struct mul_client *ci;

  ci = get_multi_katcp(d);
  if(ci == NULL){
    log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "internal logic failure, no state accessible");
    return KATCP_RESULT_FAIL;
  }

  if((argc < 2) || ((remote = arg_string_katcp(d, 1)) == NULL)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "need a remote party to connect to");
    return KATCP_RESULT_INVALID;
  }

  if(start_leaf(ci->c_loop, remote) == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to initiate connection attempt to %s", remote);
    return KATCP_RESULT_FAIL;
  }

  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "attempting to connect to %s", remote);

  return KATCP_RESULT_OK;
}

int leaf_list_cmd(struct katcp_dispatch *d, int argc)
{
  struct mul_client *ci;
  struct mul_leaf *lx;
  struct task_loop *tx;
  int count;

  ci = get_multi_katcp(d);
  if(ci == NULL){
    return KATCP_RESULT_FAIL;
  }

  tx = NULL;
  count = 0;
  while((tx = find_task_loop(ci->c_loop, ci->c_task, tx, ci->c_overall->o_type_leaf)) != NULL){
    lx = user_loop(ci->c_loop, tx);
    if(lx){
      send_katcp(d, 
          KATCP_FLAG_FIRST | KATCP_FLAG_STRING, "#leaf-list", 
          KATCP_FLAG_STRING, lx->l_name,
          KATCP_FLAG_LAST | KATCP_FLAG_STRING, lx->l_version ? : "unknown-0.0");
    }

    count++;
  }

  send_katcp(d, 
      KATCP_FLAG_FIRST | KATCP_FLAG_STRING, "!leaf-list", 
      KATCP_FLAG_STRING, KATCP_OK,
      KATCP_FLAG_LAST  | KATCP_FLAG_ULONG, (unsigned long) count);

  return KATCP_RESULT_OWN;
}
