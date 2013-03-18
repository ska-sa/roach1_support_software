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
#include "misc.h"
#include "holo.h"
#include "holo-registers.h"
#include "holo-config.h"

int toggle_bram_holo(struct katcp_dispatch *d, void *data, int start);
void destroy_bram_holo(struct katcp_dispatch *d, void *data);

#define BLOCK_SIZE 512

int create_bram_holo(struct katcp_dispatch *d, struct capture_holo *ch, unsigned int blocks)
{
  struct snap_bram *sb;
  unsigned int i;

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "snap bram:LOGIC starting");
  sb = malloc(sizeof(struct snap_bram));
  if(sb == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "snap bram:unable to allocate %d bytes for state", sizeof(struct snap_bram));
    return -1;
  }

  sb->b_capture = NULL;
  sb->b_bram = NULL;
  sb->b_blocks = blocks;

  sb->b_lost = 0;
  sb->b_got = 0;
  sb->b_txp = 0;

  sb->b_buffer = NULL;
  sb->b_old.tv_sec = 0;
  sb->b_old.tv_usec = 0;
  sb->b_now.tv_sec = 0;
  sb->b_now.tv_usec = 0;

  sb->b_bram = malloc(sizeof(struct poco_core_entry *) * blocks);
  if(sb->b_bram == NULL){
    destroy_bram_holo(d, sb);
    return -1;
  }
  sb->b_buffer = malloc(sizeof(uint32_t) * BLOCK_SIZE * blocks);/*4 * 512 * 8*/
  if(sb->b_buffer == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to allocate %d bytes for data storage buffer", sizeof(uint32_t) * BLOCK_SIZE * blocks);
    destroy_bram_holo(d, sb);
    return -1;
  }

  for(i = 0; i < blocks; i++){
    sb->b_bram[i] = NULL;
  }

  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "allocted %d bytes for snap bram", sizeof(uint32_t) * BLOCK_SIZE * blocks);

  ch->c_dump = sb;
  ch->c_toggle = &toggle_bram_holo;
  ch->c_destroy = &destroy_bram_holo;

  sb->b_capture = ch;
#ifdef DEBUG
  fprintf(stderr, "capture_bram_holo fn registered\n");
#endif
  return 0;
}

#define BRAM_PAYLOAD_SIZE 1024
int run_bram_holo(struct katcp_dispatch *d, void *data)
{
  struct snap_bram *sb;
  struct state_holo *sh;
  unsigned int i, j, k;
  uint32_t *acc_output;
  uint32_t control, status, ts_1, ts_2, ts_exp, stamp;
  unsigned int total;
  unsigned int no_of_packets, packet_count;
  int rr,wr,valid;
  char *bram_names[] = {ACC_BRAM1, ACC_BRAM2, ACC_BRAM3, ACC_BRAM4, ACC_BRAM5, ACC_BRAM6, ACC_BRAM7, ACC_BRAM8};

#ifdef DEBUG
  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "running bram dump function");
#endif

  sb = data;
  valid = 0;

  sh = need_current_mode_katcp(d, POCO_HOLO_MODE);
  if(sh == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "left holo mode hence disabling bram");
    return -1;
  }
  gettimeofday(&(sb->b_now), NULL);

  /* Read donebit,if set clear the done bit*/
  rr = read_pce(d, sb->b_sts, &status, 0, 4);
  if(rr != 4){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "status is 0x%02x",status);
    return -1;
  }


  if(status & (IS_DONE_VACC_STATUS)){/*Check if done bit high and data ready to be read*/
    /*clear done bit:edge triggered,active high done_rst */
    sb->b_got++;
    control = sh->h_source_state;
    wr = write_pce(d, sb->b_ctl, &control, 0, 4);
    if(wr != 4){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to zero bram control");
      return -1;
    }
    //control = sh->h_source_state | (status & (IS_DONE_VACC_STATUS));
    control = (sh->h_source_state | CLEAR_DONE);
    wr = write_pce(d, sb->b_ctl, &control, 0, 4);
    if(wr != 4){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to reset bram control");
      return -1;
    }

    /*TO DO: Read Timestamp before reading data:ts_1*/
    if(read_name_pce(d, ACC_TIMESTAMP_HOLO_REGISTER, &ts_1, 0, 4) != 4){
      return -1;
    }
    /* NOTE:structure passed pce and direct pce approach
     * structure passed more adv than direct as direct parses thru all the register list and returns a pointer
     * whereas structure passed one finds the pointer the first time and uses that everytime its called*/
    for(i = 0; i < sb->b_blocks; i++){
      sb->b_bram[i] = by_name_pce(d, bram_names[i]);
      if(sb->b_bram[i] == NULL){
        log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to locate register %s for writing", bram_names[i]);
        return -1;
      }
      rr = read_name_pce(d, bram_names[i], (sb->b_buffer + (BLOCK_SIZE * i)), 0, BLOCK_SIZE * sizeof(uint32_t) );
      if(rr < (BLOCK_SIZE * sizeof(uint32_t))){
        log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "first:unable to acquire full bram buffer of %u bytes", BLOCK_SIZE);
        return -1;
      }
      log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "read %d bytes into %p+%d from %s", rr, sb->b_buffer, BLOCK_SIZE * sizeof(uint32_t) * i, bram_names[i]);
    }
    /*TO DO: Read Timestamp after reading data:ts_2*/
    if(read_name_pce(d, ACC_TIMESTAMP_HOLO_REGISTER, &ts_2, 0, 4) != 4){
      return -1;
    }

    /*TO DO: Calculation expected timestamp:ts_e*/
    if(sb->b_first_time){
      ts_exp = 0;
      sb->b_first_time = 0;
    }else{
      ts_exp = sb->b_ts_old + sh->h_dump_count;
    }

    /*TO DO: Comparison of ts's*/
    log_message_katcp(d, KATCP_LEVEL_DEBUG,  NULL, "timestamp values before data read = %d,after data read = %d, old = %d and expected = %d", ts_1, ts_2, sb->b_ts_old, ts_exp);
    if(ts_1 != ts_2){/*corrupted*/
      valid = 0;
      sb->b_lost += ((ts_2 - sb->b_ts_old)/sh->h_dump_count);/*Not transmitting further:data corruption*/
      sb->b_ts_old = ts_2;/*Adjusting the timestamp for correction despite missed packets*/
      log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "timestamp before data read[%d] != timestamp after data read[%d] time now[%lu.%06lus] previous dump at[%lu.%06lus]",ts_1,ts_2, sb->b_now.tv_sec, sb->b_now.tv_usec, sb->b_old.tv_sec, sb->b_old.tv_usec);
    }else if((ts_1 == ts_2) && (ts_2 != ts_exp)){/*missed packet,clean*/
      valid = 1;
      sb->b_lost += ((ts_2 - ts_exp)/sh->h_dump_count);/*Not transmitting further:data corruption*/
      sb->b_ts_old = ts_2;/*Adjusting the timestamp for correction despite missed packets*/
      log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "Expected timestamp[%d] != timestamp[%d]",ts_exp,ts_2);
    }else if((ts_1 == ts_exp) && (ts_2 == ts_exp)){/*clean*/
      valid = 1;
      sb->b_ts_old = ts_2;
      //log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "valid set: ts_1 == ts_exp && ts_2 == ts_exp, clean");
    }else{
      valid = 0;
      sb->b_ts_old = ts_2;/*Adjusting the timestamp for correction despite missed packets*/
      log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "Should never happen:Old Timestamp[%d],Expected[%d], got[%d]",sb->b_ts_old, ts_exp, ts_2);
    }

  }else{
    /*Reading timestamp for setting*/
    if(read_name_pce(d, ACC_TIMESTAMP_HOLO_REGISTER, &stamp, 0, 4) != 4){
      return -1;
    }
    if(stamp < sb->b_capture->c_ts_lsw){
      //sb->b_capture->c_ts_msw++;
      sb->b_capture->c_ts_msw = 0;
    }
    sb->b_capture->c_ts_lsw = ts_2;
    log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "bram not yet ready:time = % u and status 0x%02x", stamp, status);
    return 0;
  }
  /*setting timestamp for metapacket*/
  if(ts_2 < sb->b_capture->c_ts_lsw){
    //sb->b_capture->c_ts_msw++;
    sb->b_capture->c_ts_msw = 0;
  }
  sb->b_capture->c_ts_lsw = ts_2;
  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "timestamp values SENT: msw = %d,lsw = %d, ", sb->b_capture->c_ts_msw, sb->b_capture->c_ts_lsw);
  if(valid == 1){
    k = 0;
    packet_count = 0;
    total = sb->b_blocks * BLOCK_SIZE * 4;/*(6 * 512 * 4)*/
    no_of_packets = (total / BRAM_PAYLOAD_SIZE);/*fixed no of packets*/

    /*to FIX ts=0 crashes CSS*/
    if(sb->b_capture->c_ts_lsw == 0){
      sb->b_capture->c_ts_lsw = 1;
      acc_output = (uint32_t *)data_udp_holo(d, sb->b_capture, 0, BRAM_PAYLOAD_SIZE);
      if(acc_output == NULL){
        log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "snap bram:unable to acquire payload space of %d",BRAM_PAYLOAD_SIZE);
        return 0;
      }
      sb->b_capture->c_ts_lsw = 0;
    }else{
      acc_output = (uint32_t *)data_udp_holo(d, sb->b_capture, 0, BRAM_PAYLOAD_SIZE);
      if(acc_output == NULL){
        log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "snap bram:unable to acquire payload space of %d",BRAM_PAYLOAD_SIZE);
        return 0;
      }
    }
#if 0
    /*fixed frame packets sent*/
    acc_output = (uint32_t *)data_udp_holo(d, sb->b_capture, 0, BRAM_PAYLOAD_SIZE);
    if(acc_output == NULL){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "snap bram:unable to acquire payload space of %d",BRAM_PAYLOAD_SIZE);
      return 0;
    }
#endif
    for(i = 0; i < BLOCK_SIZE; i++){
      for(j = 0; j < sb->b_blocks; j++){
        acc_output[k] = (sb->b_buffer[i + (j * BLOCK_SIZE)]);
        k++;
        if((k * 4) == BRAM_PAYLOAD_SIZE){
          /*to FIX ts=0 crashes CSS*/
          tx_udp_holo(d, sb->b_capture);
          sb->b_txp++;
          log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "tx holo packets,offset=%d length=%d",packet_count * BRAM_PAYLOAD_SIZE ,BRAM_PAYLOAD_SIZE);
          packet_count++;
          if(packet_count != no_of_packets){
            if(sb->b_capture->c_ts_lsw == 0){
              sb->b_capture->c_ts_lsw = 1;
              acc_output = (uint32_t *)data_udp_holo(d, sb->b_capture, (packet_count * BRAM_PAYLOAD_SIZE), BRAM_PAYLOAD_SIZE);
              if(acc_output == NULL){
                log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "SPOTTED:snap bram:unable to acquire payload space of %d",BRAM_PAYLOAD_SIZE);
                return 0;
              }
              sb->b_capture->c_ts_lsw = 0;
            }else{
              acc_output = (uint32_t *)data_udp_holo(d, sb->b_capture, (packet_count * BRAM_PAYLOAD_SIZE), BRAM_PAYLOAD_SIZE);
              if(acc_output == NULL){
                log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "SPOTTED:snap bram:unable to acquire payload space of %d",BRAM_PAYLOAD_SIZE);
                return 0;

              }
            }
            k = 0;
          }
        }
      }
    }
    }
    //sb->b_ts_old = ts_exp;
    /*TO DO: Check whether now value gets set in old;integrate into the structure for reuse*/
    sb->b_old = sb->b_now;
    return 0;
  }

int toggle_bram_holo(struct katcp_dispatch *d, void *data, int start)
  {
  struct snap_bram *sb;
  struct state_holo *sh;
  uint32_t value;
  struct timeval period, now;
  unsigned int rst_value;
  unsigned int control;
  int wr;

  sb = data;

  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "toggling bram capture %s", start ? "on" : "off");

  if(start == 0){ /* stop */

    log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "read %u valid bram blocks", sb->b_got);
    sb->b_got = 0;

    log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "encountered %u lost accumulations", sb->b_lost);
    sb->b_lost = 0;

    log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "sent %u bram data frames", sb->b_txp);
    sb->b_txp = 0;
    /* TODO - have an up flag to check if not shutting something which isn't running */

    rst_value = 0x1;
    if(write_name_pce(d, CONTROL_HOLO_REGISTER, &(rst_value), 0, 4) != 4){
      return -1;
    }
    sb->b_ts_old = 0;

    discharge_timer_katcp(d, sb);
    return 0;
  }
  sh = get_mode_katcp(d, POCO_HOLO_MODE);
  if(sh == NULL){
    return -1;
  }
/**********************************************************************/
  /* reset the whole system before capture starts */
  rst_value = 0x1;
  if(write_name_pce(d, CONTROL_HOLO_REGISTER, &(rst_value), 0, 4) != 4){
    return -1;
  }
/**********************************************************************/

  sb->b_sts = by_name_pce(d, STATUS_HOLO_REGISTER);
  sb->b_ctl = by_name_pce(d, CONTROL_HOLO_REGISTER);
  sb->b_stamp = by_name_pce(d, ACC_TIMESTAMP_HOLO_REGISTER);
  if(!(sb->b_ctl && sb->b_sts && sb->b_stamp)){
#ifdef DEBUG
    fprintf(stderr, "bram: unable to locate registers\n");
#endif
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "bram unable to locate registers");
    return -1;
  }

  /* set first time timestamp flag*/
  sb->b_first_time = 1;
  /* set accumulation length */
  if(write_name_pce(d, ACC_LEN_HOLO_REGISTER, &(sh->h_dump_count), 0, 4) != 4){
    return -1;
  }
#if 0
  /* DEBUG ONLY: check accumulator status */
  if(read_name_pce(d, ACC_TIMESTAMP_HOLO_REGISTER, &value, 0, 4) != 4){
    return -1;
  }
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "Before reset ts_val = %d", value);
#endif
  /* Clear the status bits before applying sys reset:overflow and old acc length values*/
  /* Write positive edge to determined control register to release the value*/
  control = 0;
  wr = write_name_pce(d, CONTROL_HOLO_REGISTER, &control, 0, 4);
  if(wr != 4){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to zero bram control");
    return -1;
  }
  control = (sh->h_source_state | CLEAR_DONEBIT_OVERFLOW | CLEAR_OVERRANGE_UNUSED);
  wr = write_name_pce(d, CONTROL_HOLO_REGISTER, &control, 0, 4);
  if(wr != 4){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to reset bram control");
    return -1;
  }
  
  /*Add the sync time*/
  gettimeofday(&now, NULL);
  sh->h_sync_time.tv_sec = now.tv_sec;
  sh->h_sync_time.tv_usec = now.tv_usec;

  /* Write 0 to sys reset to start vacc*/
  rst_value = 0x0;
  if(write_name_pce(d, CONTROL_HOLO_REGISTER, &(rst_value), 0, 4) != 4){
    return -1;
  }
#if 0
  /* DEBUG ONLY: check accumulator status */
  if(read_name_pce(d, ACC_TIMESTAMP_HOLO_REGISTER, &value, 0, 4) != 4){
    return -1;
  }

  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "After reset ts_val = %d", value);
#endif
  /*Try polling more frequently by changing it to 4*/
  value = sh->h_accumulation_length / 2;
  period.tv_sec = value / MSECPERSEC_HOLO;
  period.tv_usec = (value % MSECPERSEC_HOLO) * MSECPERSEC_HOLO;
  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "setting poll interval to %lu.%06lus for accumulation length of %ums", period.tv_sec, period.tv_usec, sh->h_accumulation_length);
   sb->b_capture->c_ts_msw = 0;
   sb->b_capture->c_ts_lsw = 0;

  if(register_every_tv_katcp(d, &period, &run_bram_holo, sb) < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to register bram collection timer");
    return -1;
  }

  return 0;
}

void destroy_bram_holo(struct katcp_dispatch *d, void *data)
{
  struct snap_bram *sb;

  if(data == NULL){
    return;
  }
  sb = data;

  if(sb->b_buffer){
    free(sb->b_buffer);
    sb->b_buffer = NULL;
  }
  sb->b_blocks = 0;

  if(sb->b_capture){
#ifdef DEBUG
    if(sb->b_capture->c_dump != NULL){
      if(sb->b_capture->c_dump != sb){
        fprintf(stderr, "bram: major logic failure: crosslinked bram and capture objects\n");  
        abort();
      }
    }
#endif
    sb->b_capture->c_dump = NULL;
    sb->b_capture->c_toggle = NULL;
    sb->b_capture = NULL;
  }

  if(sb->b_bram){
    free(sb->b_bram);
    sb->b_bram = NULL;
  }

  discharge_timer_katcp(d, sb);

  free(sb);
}
