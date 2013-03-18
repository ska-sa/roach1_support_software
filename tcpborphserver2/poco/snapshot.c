
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
#include "config.h"
#include "registers.h"
#include "input.h"

/*
   ?poco-snap-shot name (parameters)
   retrieve a block of data at the named point in the processing pipeline. For the adc snapshot, a single parameter
   describes the input-polarisation, eg snap-shot adc input-polarisation
 */

#define NAME_BUFFER 128
#define LENGTH      (2048*4)
#define WAIT_THRESH  4 

int wait_address_tick(struct katcp_dispatch *d, void *data)
{
  unsigned int i;
  struct katcp_dispatch *dx;
  uint32_t value;
  int rr;
  char *data_local;

  dx = data;

  for(i = 0; i <= WAIT_THRESH; i++){
    rr = read_name_pce(d, SNAP_ADDR_POCO_REGISTER, &value, 0, 4);
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

  rr = read_name_pce(d, SNAP_BRAM_LSB_POCO_REGISTER, data_local, 0, LENGTH);

  if(rr == LENGTH){
    append_string_katcp(dx, KATCP_FLAG_FIRST | KATCP_FLAG_STRING, "!poco-snap-shot");
    append_string_katcp(dx, KATCP_FLAG_STRING, KATCP_OK);
    append_buffer_katcp(dx, KATCP_FLAG_LAST | KATCP_FLAG_BUFFER, data_local, LENGTH);
    log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "rr=LENGTH");

  } else {
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "read %d instead of %d bytes", rr, LENGTH);
    extra_response_katcp(d, KATCP_RESULT_FAIL, "read");
  }

  free(data_local);

  return KATCP_RESULT_OWN;
}

int poco_snap_shot_cmd(struct katcp_dispatch *d, int argc)
{
  struct state_poco *sp;
  struct poco_core_entry *control0;
  struct timeval now;
  unsigned int i,flag;
  uint32_t ctrl[2];
  uint32_t value;
  char *name, *label;
  char reply[NAME_BUFFER];
  int inp, pol;
  int  wr;

  sp = need_current_mode_katcp(d, POCO_POCO_MODE);
  if(sp == NULL){
    return KATCP_RESULT_FAIL;
  }

  if(argc <= 2){
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
  label = arg_string_katcp(d, 2);

  if((name == NULL) || (label == NULL)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to acquire parameters");
    extra_response_katcp(d, KATCP_RESULT_FAIL, "internal");
    return KATCP_RESULT_OWN;
  }

  inp = extract_input_poco(label);
  pol = extract_polarisation_poco(label);
  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "name=%s,inp=%d,pol=%d",name,inp,pol);

  if(strcmp(name, "adc") == 0){
    flag = SOURCE_ADC_CONTROL0;
  } else if(strcmp(name, "quant") == 0){
    flag = SOURCE_QUANT_CONTROL0;
  } else {
    flag = 0; /* WARNING: unclear here */
  }

  /*Logic determining the register to write based on input and polarisation*/
  if(!inp && !pol){
    inp = 0;
    flag |= SOURCE0_CONTROL0;
  }
  else if(!inp && pol){
    inp = 1;
    flag |= SOURCE1_CONTROL0;
  }
  else if(inp && !pol){
    inp = 2;
    flag |= SOURCE2_CONTROL0;
  }
  else{
    inp = 3;
    flag |= SOURCE3_CONTROL0;
  }

  control0 = by_name_pce(d, CONTROL0_POCO_REGISTER);

  if(control0 == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to locate register %s", CONTROL0_POCO_REGISTER);
    return -1;
  }


  value = sp->p_source_state;                                                    
  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "sanity check:Before flags applied ctrl0 = %x", value);

  /* Masking and setting required bits for source */
  value = (CLEAR_SOURCE_CONTROL0(value) | flag);
  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "sanity check:After flags applied ctrl0 = %x", value);

  sp->p_source_state = value;                                                    

#if 0
  rr = read_name_pce(d, CONTROL0_POCO_REGISTER, &value, 0, 4);
  if(rr != 4){
    return -1;
  }
  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "sanity check:Before flags applied ctrl0 = %x", value);
#endif

  if(write_pce(d, control0, &value, 0, 4) != 4){
    return -1;
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
    ctrl[1] = (0x1 | TRIGGER_SOURCE);
  }
  else{
    ctrl[1] = 0x1; 
  }
  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "sanity check:snap_ctrl = %x", ctrl[1]);

  for(i = 0; i < 2; i++){
    wr = write_name_pce(d, SNAP_CTRL_POCO_REGISTER, &ctrl[i], 0, 4);
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
  register_at_tv_katcp(d, &now, &wait_address_tick, d); 

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
