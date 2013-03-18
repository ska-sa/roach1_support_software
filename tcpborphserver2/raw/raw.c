#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <dirent.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <katcp.h>

#include "core.h"
#include "raw.h"
#include "misc.h"
#include "modes.h"

void destroy_raw_poco(struct katcp_dispatch *d)
{
  struct state_raw *sr;

  sr = get_mode_katcp(d, POCO_RAW_MODE);
  if(sr == NULL){
    return;
  }

  if(sr->s_bof_path){
    free(sr->s_bof_path);
    sr->s_bof_path = NULL;
  }

  free(sr);
}

int display_dir_poco_cmd(struct katcp_dispatch *d, char *path)
{
  DIR *dr;
  struct dirent *de;
  char *label;
  unsigned long count;

  label = arg_string_katcp(d, 0);
  if(label == NULL){
    log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "internal logic failure");
    return KATCP_RESULT_FAIL;
  }

  dr = opendir(path);
  if(dr == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to open %s: %s", path, strerror(errno));
    extra_response_katcp(d, KATCP_RESULT_FAIL, "directory");
    return KATCP_RESULT_OWN;
  }

  count = 0;

  while((de = readdir(dr)) != NULL){
    if(de->d_name[0] != '.'){ /* ignore hidden files */
      send_katcp(d,
          KATCP_FLAG_FIRST | KATCP_FLAG_STRING | KATCP_FLAG_MORE, "#", 
          KATCP_FLAG_STRING, label + 1,
          KATCP_FLAG_LAST | KATCP_FLAG_STRING, de->d_name);
      count++;
    }
  }

  closedir(dr);

  send_katcp(d,
      KATCP_FLAG_FIRST | KATCP_FLAG_STRING | KATCP_FLAG_MORE, "!", 
      KATCP_FLAG_STRING, label + 1,
      KATCP_FLAG_STRING, KATCP_OK, 
      KATCP_FLAG_LAST | KATCP_FLAG_ULONG, count);

  return KATCP_RESULT_OWN;
}

int read_cmd(struct katcp_dispatch *d, int argc)
{
  struct state_raw *sr;
  struct poco_core_entry *pce;
  unsigned int start, length;
  char *data, *name;
  int rr;

  sr = get_current_mode_katcp(d);
  if(sr == NULL){
    log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "unable to get raw state");
    return KATCP_RESULT_FAIL;
  }

#if 0
  name = arg_string_katcp(d, 0);
  if(name == NULL){
    log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "internal logic failure - unable to access request name");
    return KATCP_RESULT_FAIL;
  }
  strncpy(reply, name, NAME_BUFFER - 1);
  reply[NAME_BUFFER - 1] = '\0';
  reply[0] = KATCP_REPLY;
#endif

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

  length = sizeof(unsigned int);
  if(argc > 3){
    length = arg_unsigned_long_katcp(d, 3);
  }

  if(length <= 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "length has to be nonzero");
    extra_response_katcp(d, KATCP_RESULT_INVALID, "length");
    return KATCP_RESULT_OWN;
  }

  if(sr->s_align_check){
    if((length & (sizeof(unsigned int) - 1)) || (start & (sizeof(unsigned int) - 1))){
      log_message_katcp(d, (sr->s_align_check > 1) ? KATCP_LEVEL_ERROR : KATCP_LEVEL_WARN, NULL, "start %u and length %u should be word aligned", start, length);
      if(sr->s_align_check > 1){
        extra_response_katcp(d, KATCP_RESULT_INVALID, "alignment");
        return KATCP_RESULT_OWN;
      }
    }
  }

  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "reading %u from %s at %u", length, name, start);

  data = malloc(length);
  if(data == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to allocate %d bytes for read buffer", length);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "allocation");
    return KATCP_RESULT_OWN;
  }

  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "allocated %d bytes for read buffer", length);

  rr = read_pce(d, pce, data, start, length);

  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "read %d bytes into buffer", rr);

  if(rr == length){
    prepend_reply_katcp(d);

    append_string_katcp(d, KATCP_FLAG_STRING, KATCP_OK);
    rr = append_buffer_katcp(d, KATCP_FLAG_LAST | KATCP_FLAG_BUFFER, data, length);

    log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "escaped data into message of %d bytes", rr);

  } else {
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "read %d instead of %d bytes", rr, length);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "read");
  }

  free(data);

  return KATCP_RESULT_OWN;
}

int bulkread_cmd(struct katcp_dispatch *d, int argc)
{
#define OUTPUT_LIMIT  (1024 * 1024)
#define OUTPUT_BUFFER 4096
  struct state_raw *sr;
  struct poco_core_entry *pce;
  unsigned long start, length, want, got, size;
  char *name;
  int rr;
  char buffer[OUTPUT_BUFFER];

  sr = get_current_mode_katcp(d);
  if(sr == NULL){
    log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "unable to get raw state");
    return KATCP_RESULT_FAIL;
  }

#if 0
  name = arg_string_katcp(d, 0);
  if(name == NULL){
    log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "internal logic failure - unable to access request name");
    return KATCP_RESULT_FAIL;
  }
  strncpy(reply, name, NAME_BUFFER - 1);
  reply[NAME_BUFFER - 1] = '\0';
  reply[0] = KATCP_REPLY;
#endif

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

  size = size_pce(pce);

  start = 0;
  if(argc > 2){
    start = arg_unsigned_long_katcp(d, 2);
    if(start >= size){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "offset %lu beyond end of file", start);
      extra_response_katcp(d, KATCP_RESULT_INVALID, "length");
      return KATCP_RESULT_OWN;
    }
  }

  length = sizeof(unsigned int);
  if(argc > 3){
    length = arg_unsigned_long_katcp(d, 3);
    if((start + length) > size){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "read of %lu@%lu exceeds register size %lu", length, start, size);
      extra_response_katcp(d, KATCP_RESULT_INVALID, "length");
      return KATCP_RESULT_OWN;
    }
  } else {
    length = size - start;
  }

  if(length <= 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "length has to be nonzero");
    extra_response_katcp(d, KATCP_RESULT_INVALID, "length");
    return KATCP_RESULT_OWN;
  }

  if(sr->s_align_check){
    if((length & (sizeof(unsigned int) - 1)) || (start & (sizeof(unsigned int) - 1))){
      log_message_katcp(d, (sr->s_align_check > 1) ? KATCP_LEVEL_ERROR : KATCP_LEVEL_WARN, NULL, "start %lu and length %lu should be word aligned", start, length);
      if(sr->s_align_check > 1){
        extra_response_katcp(d, KATCP_RESULT_INVALID, "alignment");
        return KATCP_RESULT_OWN;
      }
    }
  }

  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "reading %lu from %s at %lu", length, name, start);

  got = 0;
  do {
    want = length - got;
    if(want > OUTPUT_BUFFER) {
      want = OUTPUT_BUFFER;
    }

    if(flushing_katcp(d) < OUTPUT_LIMIT){
      rr = read_pce(d, pce, buffer, start + got, want);
      if(rr != want){
        log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "incomplete read position %lu", rr, start + got + ((rr > 0) ? rr : 0));
        extra_response_katcp(d, KATCP_RESULT_FAIL, "read");
        return KATCP_RESULT_OWN;
      }

      prepend_inform_katcp(d);
      append_buffer_katcp(d, KATCP_FLAG_LAST | KATCP_FLAG_BUFFER, buffer, rr);

      got += rr;
    }

    /* actually makes things go more slowly, but there to make traffic less spikey */
    if(write_katcp(d) < 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to write bulkread data");
      return KATCP_RESULT_FAIL;
    }

  } while(got < length);


  prepend_reply_katcp(d);
  append_string_katcp(d, KATCP_FLAG_STRING, KATCP_OK);
  append_unsigned_long_katcp(d, KATCP_FLAG_LAST | KATCP_FLAG_ULONG, (unsigned long) got);

  return KATCP_RESULT_OWN;
#undef OUTPUT_BUFFER
}

int write_cmd(struct katcp_dispatch *d, int argc)
{
  struct state_raw *sr;
  struct poco_core_entry *pce;
  unsigned int start, length;
  char *data, *name;
  int wr;
#if 0
  char reply[NAME_BUFFER];
#endif

  sr = get_current_mode_katcp(d);
  if(sr == NULL){
    log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "unable to get raw state");
    return KATCP_RESULT_FAIL;
  }

  if(argc <= 3){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "need a register, offset and data to write");
    return KATCP_RESULT_INVALID;
  }


#if 0
  name = arg_string_katcp(d, 0);
  if(name == NULL){
    log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "internal logic failure - unable to access request name");
    return KATCP_RESULT_FAIL;
  }
  strncpy(reply, name, NAME_BUFFER - 1);
  reply[NAME_BUFFER - 1] = '\0';
  reply[0] = KATCP_REPLY;
#endif

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
    return KATCP_RESULT_OWN;
  }

  start = arg_unsigned_long_katcp(d, 2);//postion where to start in borph file
  length = arg_buffer_katcp(d, 3, NULL, 0);
  data = arg_string_katcp(d, 3); /* WARNING: too chummy with API internals */

  if(sr->s_align_check){
    if((length & (sizeof(unsigned int) - 1)) || (start & (sizeof(unsigned int) - 1))){
      log_message_katcp(d, (sr->s_align_check > 1) ? KATCP_LEVEL_ERROR : KATCP_LEVEL_WARN, NULL, "start %u and length %u have to be word aligned", start, length);
      if(sr->s_align_check > 1){
        extra_response_katcp(d, KATCP_RESULT_INVALID, "alignment");
        return KATCP_RESULT_OWN;
      }
    }
  }

  if(length <= 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "cowardly refusing to write empty parameter");
    extra_response_katcp(d, KATCP_RESULT_INVALID, "length");
    return KATCP_RESULT_OWN;
  }

  wr = write_pce(d, pce, data, start, length); /* writing to borph register */

  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "wrote %d/%lu bytes to %s at %u", wr, length, name, start);

  if(wr != length){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "write failed %d instead of %d bytes", wr, length);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "write");
    return KATCP_RESULT_OWN;
  }

  return KATCP_RESULT_OK;
}

#define SYNC_INTERVAL (200*1000000)

int sysinit_cmd(struct katcp_dispatch *d, int argc)
{
  struct state_raw *sr;
  struct poco_core_entry *pce,*pce1;
  uint32_t sync_value, time_value;
  int wr;

  sr = get_current_mode_katcp(d);
  if(sr == NULL){
    log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "unable to get raw state");
    return KATCP_RESULT_FAIL;
  }

  pce = by_name_pce(d, "timing_ctl");//gives a structure back which is a ptr to a particular borph reg
  pce1 = by_name_pce(d, "sync_period");//gives a structure back which is a ptr to a particular borph reg

  if(pce == NULL || pce1 == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "registers not found");
    if(programmed_core_poco(d, NULL)){
      extra_response_katcp(d, KATCP_RESULT_FAIL, "register");
    } else {
      extra_response_katcp(d, KATCP_RESULT_FAIL, "program");
    }
    return KATCP_RESULT_OWN;
  }

  /*Writing to the timing_ctl register for resetting*/
  time_value = 0;
  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "writing 4 bytes to timing_ctl register");
  wr = write_pce(d, pce, &time_value, 0, 4);//writing to borph register

  if(wr < 4){
    //log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "write failed %d instead of %d bytes", wr, length);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "write");
    return KATCP_RESULT_OWN;
  }

  time_value = 0x1;
  wr = write_pce(d, pce, &time_value, 0, 4);//writing to borph register
  if(wr < 4){
    //log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "write failed %d instead of %d bytes", wr, length);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "write");
    return KATCP_RESULT_OWN;
  }

  /*Writing to the sync_register*/
  sync_value = SYNC_INTERVAL;
  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "writing 4 bytes to sync_period register");
  wr = write_pce(d, pce1, &sync_value, 0, 4);//writing to borph register

  if(wr < 4){
    //log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "write failed %d instead of %d bytes", wr, length);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "write");
    return KATCP_RESULT_OWN;
  }

  /*Writing to the timing_ctl register for starting*/
  time_value = 0;
  wr = write_pce(d, pce, &time_value, 0, 4);//writing to borph register

  if(wr < 4){
    //log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "write failed %d instead of %d bytes", wr, length);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "write");
    return KATCP_RESULT_OWN;
  }

  time_value = 0x2;
  wr = write_pce(d, pce, &time_value, 0, 4);//writing to borph register
  if(wr < 4){
    //log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "write failed %d instead of %d bytes", wr, length);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "write");
    return KATCP_RESULT_OWN;
  }

  time_value = 0;
  wr = write_pce(d, pce, &time_value, 0, 4);//writing to borph register

  if(wr < 4){
    //log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "write failed %d instead of %d bytes", wr, length);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "write");
    return KATCP_RESULT_OWN;
  }

  time_value = 0x2;
  wr = write_pce(d, pce, &time_value, 0, 4);//writing to borph register
  if(wr < 4){
    //log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "write failed %d instead of %d bytes", wr, length);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "write");
    return KATCP_RESULT_OWN;
  }

  return KATCP_RESULT_OK;
}

int delbof_cmd(struct katcp_dispatch *d, int argc)
{
  struct state_raw *sr;
  char *name, *ptr;
  int len, result;

  sr = get_current_mode_katcp(d);
  if(sr == NULL){
    log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "unable to get raw state");
    return KATCP_RESULT_FAIL;
  }

  if(argc <= 1){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "need a file to delete");
    return KATCP_RESULT_INVALID;
  }

  name = arg_string_katcp(d, 1);
  if(name == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to acquire first parameter");
    return KATCP_RESULT_FAIL;
  }

  if(strchr(name, '/') || (name[0] == '.')){
    log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "client attempts to specify a path (%s)", name);
    return KATCP_RESULT_FAIL;
  }

  len = strlen(name) + strlen(sr->s_bof_path) + 1;
  ptr = malloc(len + 1);
  if(ptr == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to allocate %d bytes", len + 1);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "allocation");
    return KATCP_RESULT_OWN;
  }

  result = snprintf(ptr, len + 1, "%s/%s", sr->s_bof_path, name);
  if(result != len){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "major logic failure: expected %d from snprintf, got %d", len, result);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "internal");
    free(ptr);
    return KATCP_RESULT_OWN;
  }

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "attempting to delete %s", ptr);
  result = unlink(ptr);

  if(result < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to delete %s: %s", name, strerror(errno));
    free(ptr);
    return KATCP_RESULT_FAIL;
  }

  free(ptr);
  return KATCP_RESULT_OK;
}

int listbof_cmd(struct katcp_dispatch *d, int argc)
{
  struct state_raw *sr;

  sr = get_current_mode_katcp(d);
  if(sr == NULL){
    log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "unable to get raw state");
    return KATCP_RESULT_FAIL;
  }

#ifdef DEBUG
  fprintf(stderr, "listbof: listing content of %s", sr->s_bof_path);
#endif

  return display_dir_poco_cmd(d, sr->s_bof_path);
}

#ifdef DEBUG
struct tick_state{
  unsigned int t_countdown;
  uint64_t t_counter;
};

int tick_run(struct katcp_dispatch *d, void *data)
{
  struct tick_state *ts;
  ts = data;

  if(ts->t_countdown <= 1){
    log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "last tick");
    free(ts);
    return -1;
  } else {
    log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "tick %u, 64bit %llu", ts->t_countdown, ts->t_counter);
    ts->t_countdown--;
    ts->t_counter += 800000000;
    return 0;
  }
}

int tick_cmd(struct katcp_dispatch *d, int argc)
{
  struct tick_state *ts;
  struct timeval tv;
  char *string;
  unsigned int count;

  if(argc <= 2){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "need a period and a number of ticks");
    return KATCP_RESULT_FAIL;
  }

  count = arg_unsigned_long_katcp(d, 2);
  string = arg_string_katcp(d, 1);

  if(string == NULL){
    log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "unable to get argument");
    return KATCP_RESULT_FAIL;
  }

  if(time_from_string(&tv, NULL, string)){
    log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "argument %s does not convert to timeval", string);
    return KATCP_RESULT_FAIL;
  }

  ts = malloc(sizeof(struct tick_state));
  if(ts == NULL){
    log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "unable to allocate");
    return KATCP_RESULT_FAIL;
  }

  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "scheduling for %u periods", count);
  ts->t_countdown = count;
  ts->t_counter = 0;

  if(register_every_tv_katcp(d, &tv, &tick_run, ts) < 0){
    free(ts);
    log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "unable to register timer");
    return KATCP_RESULT_FAIL;
  }

  return KATCP_RESULT_OK;
}

int wildcard_cmd(struct katcp_dispatch *d, int argc)
{
  char *name;

  name = arg_string_katcp(d, 0);
  if(name){
    log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "no command %s available", name);
  }

  return KATCP_RESULT_OK;
}
#endif

int progdev_cmd(struct katcp_dispatch *d, int argc)
{
  char *image, *basename;
  struct timeval tv;
  struct state_raw *sr;
  int result, len;

  tv.tv_usec = 0;
  tv.tv_sec = WAIT_END_TGTAP;

  sr = get_current_mode_katcp(d);
  if(sr == NULL){
    log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "unable to get raw state");
    return KATCP_RESULT_FAIL;
  }

  if((argc <= 1) || arg_null_katcp(d, 1)){
    image = NULL;
  } else {
    basename = arg_string_katcp(d, 1);
    if(basename == NULL){
      log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "major logic failure, image argument inaccessible");
      return KATCP_RESULT_FAIL;
    }

    if(strchr(basename, '/') || (basename[0] == '.')){
      log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "client attempts to specify a path (%s)", basename);
      return KATCP_RESULT_INVALID;
    }

    len = strlen(basename) + strlen(sr->s_bof_path) + 2;

    image = malloc(len);
    if(image == NULL){
      log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "unable to allocate %d bytes", len);
      extra_response_katcp(d, KATCP_RESULT_FAIL, "allocation");
      return KATCP_RESULT_OWN;
    }

    snprintf(image, len, "%s/%s", sr->s_bof_path, basename);
    image[len - 1] = '\0';
  }

  result = end_type_shared_katcp(d, RAW_TYPE_TGTAP, 1);
  if(result < 0){
    log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "unable to end running tap processes");
    extra_response_katcp(d, KATCP_RESULT_FAIL, "signalling");
    return KATCP_RESULT_OWN;
  }

  if(result > 0){
    log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "killed %d tap instances", result);
  }

  result = program_core_poco(d, image);
  if(image){
    free(image);
  }

  if(result < 0){
    log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "unable to %s gateware", image ? "program" : "clear");
    extra_response_katcp(d, KATCP_RESULT_FAIL, "program");
    return KATCP_RESULT_OWN;
  }

  prepend_reply_katcp(d);
  append_string_katcp(d, KATCP_FLAG_STRING, KATCP_OK);
  append_unsigned_long_katcp(d, KATCP_FLAG_LAST | KATCP_FLAG_ULONG, (unsigned long) result);

  return KATCP_RESULT_OWN;
}

int status_cmd(struct katcp_dispatch *d, int argc)
{
  if(programmed_core_poco(d, NULL)){
    extra_response_katcp(d, KATCP_RESULT_OK, "program");
  } else {
    extra_response_katcp(d, KATCP_RESULT_FAIL, "program");
  }

  return KATCP_RESULT_OWN;
}

int listdev_cmd(struct katcp_dispatch *d, int argc)
{
  struct poco_core_entry *pce;
  unsigned int i, detail;
  char *label, *name, *option;
  unsigned long size;

  detail = 0;

  if(argc > 1){
    option = arg_string_katcp(d, 1);
    if(option && !strcmp(option, "size")){
      detail = 1;
    }
  }

  label = arg_string_katcp(d, 0);
  if(label == NULL){
    log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "internal logic failure: unable to acquire request");
    return KATCP_RESULT_FAIL;
  }

  if(!programmed_core_poco(d, NULL)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "no borph process running");
    extra_response_katcp(d, KATCP_RESULT_FAIL, "program");
    return KATCP_RESULT_OWN;
  }

  for(i = 0; (pce = by_offset_pce(d, i)) != NULL; i++){
    name = name_pce(pce);
    if(name == NULL){
      log_message_katcp(d, KATCP_LEVEL_FATAL, NULL, "internal logic failure: unable to acquire register name");
      extra_response_katcp(d, KATCP_RESULT_FAIL, "register");
      return KATCP_RESULT_OWN;
    }

    if(detail){
      size = size_pce(pce);
      send_katcp(d,
          KATCP_FLAG_FIRST | KATCP_FLAG_STRING | KATCP_FLAG_MORE, "#", 
          KATCP_FLAG_STRING, label + 1,
          KATCP_FLAG_STRING, name,
          KATCP_FLAG_LAST | KATCP_FLAG_ULONG, size);
    } else {
      send_katcp(d,
          KATCP_FLAG_FIRST | KATCP_FLAG_STRING | KATCP_FLAG_MORE, "#", 
          KATCP_FLAG_STRING, label + 1,
          KATCP_FLAG_LAST | KATCP_FLAG_STRING, name);
    }
  }
  
  return KATCP_RESULT_OK;
}

int setup_raw_poco(struct katcp_dispatch *d, char *bofs, int check)
{
  struct state_raw *sr;
  int result;

  if(bofs == NULL){
    return -1;
  }

  sr = malloc(sizeof(struct state_raw));
  if(sr == NULL){
    return -1;
  }

  sr->s_align_check = check;
  sr->s_bof_path = strdup(bofs);
  if(sr->s_bof_path == NULL){
    free(sr);
    return -1;
  }

  /* register raw mode */

  if(store_full_mode_katcp(d, POCO_RAW_MODE, POCO_RAW_NAME, NULL, NULL, sr, &destroy_raw_poco) < 0){
    free(sr->s_bof_path);
    free(sr);
#ifdef DEBUG
    fprintf(stderr, "poco: unable to register raw mode\n");
#endif
    return -1;
  }

  result = 0;

  result += register_mode_katcp(d, "?progdev",    "programs a gateware image (?progdev [image-file])", &progdev_cmd, POCO_RAW_MODE);
  result += register_mode_katcp(d, "?status",     "reports if gateware has been programmed (?status)", &status_cmd, POCO_RAW_MODE);
  result += register_mode_katcp(d, "?listbof",    "lists available gateware images (?listbof)", &listbof_cmd, POCO_RAW_MODE);
  result += register_mode_katcp(d, "?uploadbof",  "upload a gateware image (?uploadbof net-port filename [size])", &uploadbof_cmd, POCO_RAW_MODE);
  result += register_mode_katcp(d, "?delbof",     "deletes a gateware image (?delbof image-file)", &delbof_cmd, POCO_RAW_MODE);
  result += register_mode_katcp(d, "?listdev",    "lists available registers (?listdev [size])", &listdev_cmd, POCO_RAW_MODE);
  result += register_mode_katcp(d, "?echotest",   "basic network echo tester (?echotest ip-address echo-port byte-count)", &echotest_cmd, POCO_RAW_MODE);

  result += register_mode_katcp(d, "?read", "reads a given number of bytes from an offset in a register (?read register-name [register-offset [byte-count]])", &read_cmd, POCO_RAW_MODE);
  result += register_mode_katcp(d, "?bulkread", "reads a given number of bytes from an offset in a register in a form suitable for large transfers (?bulkread register-name [register-offset [byte-count]])", &bulkread_cmd, POCO_RAW_MODE);
  result += register_mode_katcp(d, "?write", "write a given payload to an offset in a register (?write register-name register-offset data-payload)", &write_cmd, POCO_RAW_MODE);
  result += register_mode_katcp(d, "?sysinit", "writes the timing ctl register that resets the entire system (?sysinit)", &sysinit_cmd, POCO_RAW_MODE);
  result += register_mode_katcp(d, "?wordread", "reads a word (?wordread register-name word-offset length)", &word_read_cmd, POCO_RAW_MODE);
  result += register_mode_katcp(d, "?wordwrite", "writes a word (?wordwrite register-name word-offset payload ...)", &word_write_cmd, POCO_RAW_MODE);

  result += register_mode_katcp(d, "?tap-start", "start a tgtap instance (?tap-start tap-device register-name ip-address [port [mac]])", &start_tgtap_cmd, POCO_RAW_MODE);
  result += register_mode_katcp(d, "?tap-stop", "stop a tgtap instance (?tap-stop register-name)", &stop_tgtap_cmd, POCO_RAW_MODE);

#ifdef DEBUG
  result += register_flag_mode_katcp(d, "?tick", "tests the scheduler (?tick milli-second-interval count)", &tick_cmd, KATCP_CMD_HIDDEN, POCO_RAW_MODE);
  result += register_flag_mode_katcp(d, NULL, "wildcard match", &wildcard_cmd, KATCP_CMD_WILDCARD, POCO_RAW_MODE);
#endif

  if(result < 0){
#ifdef DEBUG
    fprintf(stderr, "poco: unable to register raw mode commands\n");
#endif
    return -1;
  }

  return 0;
}
