#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <katcp.h>
#include <errno.h>

#include "core.h"

#define NAME_BUFFER 128

int word_write_cmd(struct katcp_dispatch *d, int argc)
{
  unsigned int start, total, value;
  int  wr, i;
  struct poco_core_entry *pce;
  char  *name;

  name = arg_string_katcp(d, 1);
  if(name == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "register name inaccessible");
    return KATCP_RESULT_FAIL;
  }

  pce = by_name_pce(d, name); /*gives a structure back which is a ptr to a particular borph reg */

  if(pce == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "register %s not found", name);
    if(programmed_core_poco(d, NULL)){
      extra_response_katcp(d, KATCP_RESULT_FAIL, "register");
    } else {
      extra_response_katcp(d, KATCP_RESULT_FAIL, "program");
    }
    return KATCP_RESULT_OWN;
  }

  start = arg_unsigned_long_katcp(d, 2);
  start *= 4;

  total = 0;

  for(i = 3; i < argc; i++){
    if(arg_null_katcp(d, i)){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "empty data field");
      return KATCP_RESULT_FAIL;
    }
    value = arg_unsigned_long_katcp(d, i);
    wr = write_pce(d, pce, &value, start, sizeof(unsigned int));
    if(wr != sizeof(unsigned int)){
      log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "unable to write %d bytes to %s: %s", sizeof(unsigned int), name, strerror(errno));
      extra_response_katcp(d, KATCP_RESULT_FAIL, "io");
      return KATCP_RESULT_OWN;
    }
    total += wr;
  }

  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "wrote %u bytes to %s", total, name);

  return KATCP_RESULT_OK;
}

int word_read_cmd(struct katcp_dispatch *d, int argc)
{
  struct poco_core_entry *pce;
  unsigned int start, length, bytes, actual;
  unsigned long value;
  unsigned int *data;
  char *name;
  int rr, have, i, flags;

  if(argc <= 1){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "need a register to read, followed by optional offset and count");
    return KATCP_RESULT_INVALID;
  }

  name = arg_string_katcp(d, 1);
  if(name == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "register name inaccessible");
    return KATCP_RESULT_FAIL;
  }

  pce = by_name_pce(d, name);
  if(pce == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "register %s not found", name);
    if(programmed_core_poco(d, NULL)){
      extra_response_katcp(d, KATCP_RESULT_FAIL, "register");
    } else {
      extra_response_katcp(d, KATCP_RESULT_FAIL, "program");
    }
    return KATCP_RESULT_OWN;
  }

  start = 0;
  if(argc > 2){
    start = arg_unsigned_long_katcp(d, 2);
  }

  length = 1; 
  if(argc > 3){
    length = arg_unsigned_long_katcp(d, 3);
  }

  if(length <= 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "length has to be nonzero");
    extra_response_katcp(d, KATCP_RESULT_INVALID, "length");
    return KATCP_RESULT_OWN;
  }

  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "reading %u from %s at %u", length, name, start);

  /* a word is 4 bytes long */
  bytes = sizeof(unsigned int) * length;
  start  *= sizeof(unsigned int);

  data = malloc(bytes);
  if(data == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to allocate %d bytes for read buffer", bytes);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "allocation");
    return KATCP_RESULT_OWN;
  }

  have = 0;
  do{
    rr = read_pce(d, pce, data, have, bytes - have);

    log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "read from %s: result=%d, wanted=%d, had=%d", name, rr, bytes - have, have);

    switch(rr){
      case -1 :
        switch(errno){
          case EAGAIN :
          case EINTR  :
            rr = 0; /* not a critical problem */
            break;
          default :
            log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "read failure on %s: %s", name, strerror(errno));
            break;
        }
        break;
      case  0 :
      default :
        have += rr;
        break;
    }
  } while((have < bytes) && (rr > 0));

  if(rr < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "read in %s failed: %s", name, strerror(errno));
    free(data);

    extra_response_katcp(d, KATCP_RESULT_INVALID, "io");
    return KATCP_RESULT_OWN;
  }

  actual = bytes / sizeof(unsigned int);
  if(actual <= 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to read a full word from %s", name);
    extra_response_katcp(d, KATCP_RESULT_INVALID, "io");
    return KATCP_RESULT_OWN;
  }

  prepend_reply_katcp(d);

  if(actual < length){
    append_string_katcp(d, KATCP_FLAG_STRING, "partial");
  } else {
    append_string_katcp(d, KATCP_FLAG_STRING, KATCP_OK);
  }

  i = 0;
  flags = KATCP_FLAG_XLONG;
  while(i < actual){
    value = data[i];
    i++;
    if(i >= length){
      flags |= KATCP_FLAG_LAST;
    }
    append_hex_long_katcp(d, flags, value);
  }

  free(data);

  return KATCP_RESULT_OWN;
}
