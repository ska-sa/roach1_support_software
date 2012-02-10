#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <katcp.h>
#include <errno.h>

#include "core.h"

#define NAME_BUFFER 128

int word_write_cmd(struct katcp_dispatch *d, int argc)
{
  unsigned int start, total, uivalue;
  int  wr, i;
  struct state_raw *sr;
  struct poco_core_entry *pce;
  char  *name;
  char reply[NAME_BUFFER];

  sr = get_current_mode_katcp(d);
  if(sr == NULL){
    log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "unable to get raw state");
    return KATCP_RESULT_FAIL;
  }

  name = arg_string_katcp(d, 0);
  if(name == NULL){
    log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "internal logic failure - unable to access request name");
    return KATCP_RESULT_FAIL;
  }
  strncpy(reply, name, NAME_BUFFER - 1);
  reply[NAME_BUFFER - 1] = '\0';
  reply[0] = KATCP_REPLY;

  name = arg_string_katcp(d, 1);
  if(name == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "register name inaccessible");
    return KATCP_RESULT_FAIL;
  }

  pce = by_name_pce(d, name);//gives a structure back which is a ptr to a particular borph reg
  if(pce == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "register %s not found", name);
    if(programmed_core_poco(d, NULL)){
      extra_response_katcp(d, KATCP_RESULT_FAIL, "register");
    } else {
      extra_response_katcp(d, KATCP_RESULT_FAIL, "program");
    }
    append_string_katcp(d, KATCP_FLAG_FIRST | KATCP_FLAG_STRING, reply);
    append_string_katcp(d, KATCP_FLAG_STRING | KATCP_FLAG_LAST , KATCP_OK);
    return KATCP_RESULT_OWN;
  }

  start = arg_unsigned_long_katcp(d, 2);
  start *= 4;

  total = 0;

  for(i = 3; i < argc; i++){
    if(arg_null_katcp(d, i)){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "empty data field");
      return KATCP_RESULT_OWN;
    }
    uivalue = arg_unsigned_long_katcp(d, i);
    wr = write_pce(d, pce, &uivalue, start, sizeof(unsigned int));
    if(wr != sizeof(unsigned int)){
      log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "unable to write %d bytes to %s: %s", sizeof(unsigned int), name, strerror(errno));
      extra_response_katcp(d, KATCP_RESULT_FAIL, "write");
      return KATCP_RESULT_OWN;
    }
    total += wr;
  }

  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "wrote %u bytes to %s", total, name);
  append_string_katcp(d, KATCP_FLAG_FIRST | KATCP_FLAG_STRING, reply);
  append_string_katcp(d, KATCP_FLAG_STRING | KATCP_FLAG_LAST, KATCP_OK);

  return KATCP_RESULT_OWN;
}


int word_read_cmd(struct katcp_dispatch *d, int argc)
{
  struct state_raw *sr;
  struct poco_core_entry *pce;
  unsigned int start, length;
  unsigned long value;
  char *data, *name;
  int rr, have, i, flags;
  char reply[NAME_BUFFER];

  sr = get_current_mode_katcp(d);
  if(sr == NULL){
    log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "unable to get raw state");
    return KATCP_RESULT_FAIL;
  }

  name = arg_string_katcp(d, 0);
  if(name == NULL){
    log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "internal logic failure - unable to access request name");
    return KATCP_RESULT_FAIL;
  }
  strncpy(reply, name, NAME_BUFFER - 1);
  reply[NAME_BUFFER - 1] = '\0';
  reply[0] = KATCP_REPLY;

  if(argc <= 1){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "need a register to read, followed by optional offset and count");
    return KATCP_RESULT_INVALID;
  }

  name = arg_string_katcp(d, 1);
  if(name == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "register name inaccessible");
    return KATCP_RESULT_FAIL;
  }

  //log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "reading %u from %s at %u", length, name, start);

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

#if 0
  if(length <= 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "length has to be nonzero");
    extra_response_katcp(d, KATCP_RESULT_INVALID, "length");
    return KATCP_RESULT_OWN;
  }
#endif

  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "reading %u from %s at %u", length, name, start);

  /*Its a word read*/
  length *= 4;
  start  *= 4;

  data = malloc(length);
  if(data == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to allocate %d bytes for read buffer", length);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "allocation");
    return KATCP_RESULT_OWN;
  }

  have = 0;
  do{
	  if(!have){
		  have = have + start;
		  length = length + start;
	  }
	  rr = read_pce(d, pce, data , have , length - have );
	  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "read from %s: result=%d, wanted=%d, had=%d", name,
			    rr, length - have, have);

	  switch(rr){
	  case -1 :
		  switch(errno){
		  case EAGAIN :
		  case EINTR  :
			  rr = 0; /* not a critical problem */
			  break;
		  default :
			  log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "read failure on %s: %s\n", name, strerror(errno));
			  break;
		  }
		  break;
	  case  0 :
	  default :
		  have += rr;
		  break;
	  }
  } while((have < length) && (rr > 0));

  if(rr < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "read in %s failed: %s", name, strerror(errno));
    free(data);
    append_string_katcp(d, KATCP_FLAG_FIRST | KATCP_FLAG_STRING, reply);
    append_string_katcp(d, KATCP_FLAG_STRING, KATCP_FAIL);
    append_string_katcp(d, KATCP_FLAG_STRING , "read");
    append_string_katcp(d, KATCP_FLAG_LAST | KATCP_FLAG_STRING , name);
    return KATCP_RESULT_OWN;
  }

  length = have - start; /* truncate to what we actually could get */

  append_string_katcp(d, KATCP_FLAG_FIRST | KATCP_FLAG_STRING, reply);
  append_string_katcp(d, KATCP_FLAG_STRING , KATCP_OK);

  i = 0;
  flags = KATCP_FLAG_XLONG;
  while(i < length){
    /* WARNING: buffer should be aligned */
    value = *((unsigned int *)(data + i));
    i += sizeof(unsigned int);
    if(i >= length){
      flags |= KATCP_FLAG_LAST;
    }
    append_hex_long_katcp(d, flags , value);
  }

  free(data);

  return KATCP_RESULT_OWN;
}


