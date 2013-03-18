#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <netdb.h>

#include <sys/time.h>

#include <katcp.h>
#include <katpriv.h> /* for timeval arith */

#include "core.h"
#include "holo.h"
#include "misc.h"
#include "modes.h"
#include "holo-registers.h"
#include "holo-config.h"
#include "holo-options.h"


int katadc_config_cmd(struct katcp_dispatch *d, int argc)
{
  struct state_holo *sh;

  sh = need_current_mode_katcp(d, POCO_HOLO_MODE);
  if(sh == NULL){
    return KATCP_RESULT_FAIL;
  }
  /*katadc configuration*/
  fprintf(stderr,"initialising ADC in slot 0\n");
  if( katadc_init(d, 0 ) < 0){
    fprintf(stderr,"main: error initialising adc 0\n");
  }

  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "katad initialistation for slot %d done", 0);

  return KATCP_RESULT_OK;
}

