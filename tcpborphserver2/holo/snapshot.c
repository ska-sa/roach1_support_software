
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
//#include "config.h"
//#include "registers.h"
#include "holo.h"
#include "holo-config.h"
#include "holo-registers.h"
#include "input.h"

/*
   ?holo-snap-shot name 
   retrieve a block of data at the named point in the processing pipeline. For the adc snapshot, a single parameter
   describes the input-polarisation, eg snap-shot adc input-polarisation
 */

#define NAME_BUFFER 128
#define LENGTH      (2048*4)/*2048 32bit values*/
#define WAIT_THRESH  4 


int wait_address_tick_holo(struct katcp_dispatch *d, void *data)
{
    unsigned int i;
    struct katcp_dispatch *dx;
    struct state_holo *sh;
    uint32_t value;
    int rr;
    char *data_local;
    char *adc_bram[] = {SNAP_BRAM_MSB, SNAP_BRAM_LSB};

    sh = need_current_mode_katcp(d, POCO_HOLO_MODE);
    if(sh == NULL){
        return KATCP_RESULT_FAIL;
    }

    dx = data;

    for(i = 0; i <= WAIT_THRESH; i++){
        rr = read_name_pce(d, SNAP_ADDR, &value, 0, 4);
        if(rr != 4){
            return -1;
        }
        if (i == WAIT_THRESH){
            log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "Timeout on snap_addr");
            return -1;
        }
        if(value == 2047){
            log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "address ticked 2047");
            break;
        }
        i++;
    }

    data_local = malloc(LENGTH);
    if(data == NULL){
        log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to allocate %d bytes for read buffer", LENGTH);
        extra_response_katcp(d, KATCP_RESULT_FAIL, "allocation");
        return KATCP_RESULT_OWN;
    }

    rr = read_name_pce(d, adc_bram[sh->inp_select], data_local, 0, LENGTH);

    if(rr == LENGTH){
        append_string_katcp(dx, KATCP_FLAG_FIRST | KATCP_FLAG_STRING, "!holo-snap-shot");
        append_string_katcp(dx, KATCP_FLAG_STRING, KATCP_OK);
        append_buffer_katcp(dx, KATCP_FLAG_LAST | KATCP_FLAG_BUFFER, data_local, LENGTH);
        log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "rr = LENGTH = %d",rr);

    } else {
        log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "read %d instead of %d bytes", rr, LENGTH);
        extra_response_katcp(d, KATCP_RESULT_FAIL, "read");
    }

    free(data_local);
    return KATCP_RESULT_OWN;
}

int holo_snap_shot_cmd(struct katcp_dispatch *d, int argc)
{
    struct state_holo *sh;
    struct timeval now;
    unsigned int i;
    uint32_t ctrl[2];
    char *name, *label;
    char reply[NAME_BUFFER];
    int inp, pol;
    int  wr;

    sh = need_current_mode_katcp(d, POCO_HOLO_MODE);
    if(sh == NULL){
        return KATCP_RESULT_FAIL;
    }

    if(argc <= 1){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "insufficient parameters");
    extra_response_katcp(d, KATCP_RESULT_FAIL, "usage");
    return KATCP_RESULT_OWN;
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
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to acquire parameters");
    extra_response_katcp(d, KATCP_RESULT_FAIL, "internal");
    return KATCP_RESULT_OWN;
  }

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "name=%s",name);

  name = arg_string_katcp(d, 1);
  label = arg_string_katcp(d, 2);

  if((name == NULL) || (label == NULL)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to acquire parameters");
    extra_response_katcp(d, KATCP_RESULT_FAIL, "internal");
    return KATCP_RESULT_OWN;
  }

  inp = extract_input_poco(label);
  pol = extract_polarisation_poco(label);
  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "name=%s,inp=%d,pol=%d",name,inp,pol);

  if(!inp && !pol){
      sh->inp_select = 0;
  }
  else if(!inp && pol){
      sh->inp_select = 1;
  }
  else if(inp && !pol){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "Not supported in holo mode,only 0x and 0y labels");
      return KATCP_RESULT_FAIL;
  }
  else{
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "Not supported in holo mode,only 0x and 0y labels");
      return KATCP_RESULT_FAIL;
  }


  /************************************************************************************
    snap_ctrl
    bit 0         :       en           positive edge 
    bit 1         :       trigger src  level          0-syn,1-now      adc:1 quant:0
    bit 2         :       we src       level          0-ext,1-always   adc:0 quant:0
   ************************************************************************************/
#define TRIGGER_SOURCE   0x2
#define WE_SOURCE        0x4

  ctrl[0] = 0x0 ;
  if(strcmp(name, "adc") == 0){
    ctrl[1] = (0x1 | TRIGGER_SOURCE | WE_SOURCE);
  }
  else{
    ctrl[1] = 0x1; 
  }
  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "sanity check:snap_ctrl = %x", ctrl[1]);

  for(i = 0; i < 2; i++){
    wr = write_name_pce(d, SNAP_CTRL, &ctrl[i], 0, 4);
    if(wr != 4){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "snap: unable to write to control");
      return -1;
    }
  }

#if 0
  /*Usage of register_at_tv_katcp*/
  register_at_tv_katcp(d, timevalstruct, functn2call, d); 
#endif

  gettimeofday(&now, NULL);
  now.tv_sec = now.tv_sec + 1;
  register_at_tv_katcp(d, &now, &wait_address_tick_holo, d); 

#if 0
  for(i = 0; i < 16; i+=4){
    rr = read_name_pce(d, SNAP_BRAM, &value, i, 4);
    if(rr != 4){
      return -1;
      log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "BRAM val =%x", value);
    }
  }
#endif
  return KATCP_RESULT_OWN;
}
