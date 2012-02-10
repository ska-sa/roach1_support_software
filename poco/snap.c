#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <netdb.h>

#include <sys/time.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <katcp.h>
#include <katpriv.h> /* for timeval arith */

#include "core.h"
#include "modes.h"
#include "poco.h"
#include "misc.h"

#define SNAP_MAGIC 0x736e6170

int toggle_snap_poco(struct katcp_dispatch *d, void *data, int start);
void destroy_snap_poco(struct katcp_dispatch *d, void *data);

/* create is responsible for adding its functions to the cp structure */

int create_snap_poco(struct katcp_dispatch *d, struct capture_poco *cp, char *name, int count, unsigned int blocks)
{
  struct snap_poco *n;
  unsigned int i;

  if(cp->c_dump != NULL){
#ifdef DEBUG
    fprintf(stderr, "snap: capture %s already associated with a data generator\n", cp->c_name);
    abort();
#endif
    return -1;
  }

  n = malloc(sizeof(struct snap_poco));
  if(n == NULL){
#ifdef DEBUG
    fprintf(stderr, "snap: unable to allocate %d bytes for state\n", sizeof(struct snap_poco));
#endif
    return -1;
  }

  n->n_magic = SNAP_MAGIC;
  n->n_capture = NULL;

  n->n_ctrl = NULL;
  n->n_addr = NULL;
  n->n_bram = NULL;

  n->n_blocks = blocks;
  n->n_count = count;
  n->n_chunk = n->n_count * 4;
  n->n_state = 0;

  n->n_name = NULL;

  while(n->n_chunk > (cp->c_size - DATA_OVERHEAD_POCO)){
    n->n_chunk = n->n_chunk / 2;
  }
  /* TODO - imperfect chunk size, improve */
#ifdef DEBUG
  fprintf(stderr, "snap: selecting a data chunk size of %ubytes, snap is %u words\n", n->n_chunk, n->n_count);
#endif

  n->n_bram = malloc(sizeof(struct poco_core_entry *) * blocks);
  if(n->n_bram == NULL){
    destroy_snap_poco(d, n);
    return -1;
  }

  for(i = 0; i < blocks; i++){
    n->n_bram[i] = NULL;
  }

  n->n_pos = strlen(name);
  n->n_name = malloc(n->n_pos + 6);
  if(n->n_name == NULL){
#ifdef DEBUG
    fprintf(stderr, "snap: unable to allocate %d bytes for buffer\n", n->n_pos + 6);
#endif
    destroy_snap_poco(d, n);
    return -1;
  }

  strcpy(n->n_name, name);

#ifdef DEBUG
  fprintf(stderr, "snap: created instance for %s\n", name);
#endif

  cp->c_dump = n;
  cp->c_toggle = &toggle_snap_poco;
  cp->c_destroy = &destroy_snap_poco;

  n->n_capture = cp;

  return 0;
}

int run_snap_poco(struct katcp_dispatch *d, void *data)
{
  struct snap_poco *n;
  struct capture_poco *cp;
  uint32_t addr, ctrl[2];
  int rr, wr, i, sr;
  void *payload;
  unsigned int total;

  n = data;

  if(need_current_mode_katcp(d, POCO_POCO_MODE) == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "left poco mode hence disabling %s snap", n->n_name);
    return -1;
  }

#ifdef DEBUG
  if(n->n_magic != SNAP_MAGIC){
    fprintf(stderr, "snap: bad magic: got 0x%x\n", n->n_magic);
    abort();
  }
#endif

  cp = n->n_capture;

  rr = read_pce(d, n->n_addr, &addr, 0, 4);
  if(rr != 4){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to read snap address of %s", n->n_name);
    return -1;
  }

  if((addr + 1) < n->n_count){
    log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "snap has %u waiting for %u", addr, n->n_count);
    return 0;
  }

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "snap got full %u+1 words", addr);

  total = n->n_count * 4;

  for(i = 0; i < total; i += n->n_chunk){
    sr = i - total;
    if(sr > n->n_chunk){
      sr = n->n_chunk;
    }

    payload = data_udp_poco(d, cp, i, sr);
    if(payload == NULL){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "snap: unable to acquire payload space of %d", sr);
      return -1;
    }

    /* TODO: add support to read from multiple blockrams */
    if(n->n_blocks == 1){
      rr = read_pce(d, n->n_bram[0], payload, i, sr);
      if(rr < sr){
        log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "snap: blockram read failed");
        return -1;
      }
    } else {
#if 0
      fprintf(stderr, "snap: multiple blocks read still to be implemented\n");
      for(i = 0; i < n->n_blocks; i++){
          rr = read_pce(d, n->n_bram[i], payload, i, sr);
          if(rr < sr){
            log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "snap: blockram read failed");
            return -1;
          }
        }
      }
#endif
    }

    tx_udp_poco(d, cp);
  }

  /* restart next capture */

  ctrl[0] = 0;
  ctrl[1] = 0x3;
  for(i = 0; i < 2; i++){
    wr = write_pce(d, n->n_ctrl, &ctrl[i], 0, 4);
    if(wr != 4){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "snap: unable to write to control");
      return -1;
    }
  }

  return 0;
}

int toggle_snap_poco(struct katcp_dispatch *d, void *data, int start)
{
  struct snap_poco *n;
  struct timeval period;
#if 0
  char name_buffer[7];
#endif
  uint32_t ctrl[2];
  int wr, i;

  n = data;

#ifdef DEBUG
  if(n->n_magic != SNAP_MAGIC){
    fprintf(stderr, "snap: bad magic: got 0x%x\n", n->n_magic);
    abort();
  }
#endif

  if(start == 0){ /* stop */
    discharge_timer_katcp(d, n);
    return 0;
  }

  period.tv_sec = 1;
  period.tv_usec = 0;

  /* else start */

  strcpy(n->n_name + n->n_pos, "_ctrl");
  n->n_ctrl = by_name_pce(d, n->n_name);

  strcpy(n->n_name + n->n_pos, "_addr");
  n->n_addr = by_name_pce(d, n->n_name);

  if(!(n->n_ctrl && n->n_addr)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to locate control registers for %s", n->n_name);
    return -1;
  }

  if(n->n_blocks == 1){
    strcpy(n->n_name + n->n_pos, "_bram");
    n->n_bram[0] = by_name_pce(d, n->n_name);
  } else {
    fprintf(stderr, "snap: multiple blocks still to be implemented\n");
#if 0
    for(i = 0; i < n->n_blocks; i++){
      snprintf(name_buffer, BUFFER_SIZE, "_bram%d", i);
      strcpy(n->n_name + n->n_pos, name_buffer);
      n->n_bram[i] = by_name_pce(d, n->n_name);
    }
#endif
    abort();
  }

  for(i = 0; i < n->n_blocks; i++){
    if(n->n_bram[i] == NULL){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to find snap block %u", i);
      return -1;
    }
  }

  ctrl[0] = 0;
  ctrl[1] = 0x3;
  for(i = 0; i < 2; i++){
    wr = write_pce(d, n->n_ctrl, &ctrl[i], 0, 4);
    if(wr != 4){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "snap: unable to write to control");
      return -1;
    }
  }

  if(register_every_tv_katcp(d, &period, &run_snap_poco, n) < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "snap: unable to register snap timer");
    return -1;
  }

  return 0;
}

void destroy_snap_poco(struct katcp_dispatch *d, void *data)
{
  struct snap_poco *n;

  if(data == NULL){
    return;
  }

  n = data;

#ifdef DEBUG
  if(n->n_magic != SNAP_MAGIC){
    fprintf(stderr, "snap: bad magic: got 0x%x\n", n->n_magic);
    abort();
  }
#endif

  if(n->n_capture){
#ifdef DEBUG
    if(n->n_capture->c_dump != NULL){
      if(n->n_capture->c_dump != n){
        fprintf(stderr, "snap: major logic failure: crosslinked snap and capture objects\n");
        abort();
      }
    }
#endif
    n->n_capture->c_dump = NULL;
    n->n_capture->c_toggle = NULL;
    n->n_capture = NULL;
  }

  if(n->n_name){
    free(n->n_name);
    n->n_name = NULL;
  }

  if(n->n_bram){
    free(n->n_bram);
    n->n_bram = NULL;
  }
  n->n_blocks = 0;

  discharge_timer_katcp(d, n);
}
