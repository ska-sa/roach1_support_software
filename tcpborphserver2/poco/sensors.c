#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/stat.h>

#include <katcp.h>
#include <katsensor.h>

#include "poco.h"
#include "registers.h"
#include "modes.h"
#include "misc.h"
#include "input.h"

/* adc sensor stuff **************************************************/

int acquire_overflow_poco(struct katcp_dispatch *d, void *data)
{
  struct state_poco *sp;
  struct overflow_sensor_poco *as;
  uint32_t value;
  int result, rr, wr;

  as = data;
  sp = need_current_mode_katcp(d, POCO_POCO_MODE);

  if(sp == NULL){
    as->a_status = NULL;
    as->a_control = NULL;
    log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "poco mode not available");
    return -1;
  }

  if(as->a_status == NULL){
    as->a_status = by_name_pce(d, STATUS_POCO_REGISTER);
    if(as->a_status == NULL){
      log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "unable to get register for name %s", STATUS_POCO_REGISTER);
      return -1;
    }
  }

  if(as->a_control == NULL){
    as->a_control = by_name_pce(d, CONTROL_POCO_REGISTER);
    if(as->a_control == NULL){
      log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "unable to get register for name %s", STATUS_POCO_REGISTER);
      return -1;
    }
  }

  rr = read_pce(d, as->a_status, &value, 0, 4);
  if(rr != 4){
    log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "read on status register returned %d", rr);
    return -1;
  }
  result = value;

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "got status word 0x%08x", result);


  value = 0;
  wr = write_pce(d, as->a_control, &value, 0, 4);

  value = CLEAR_ALL_CONTROL;
  wr = write_pce(d, as->a_control, &value, 0, 4);
  /* TODO: do something with return code ? */

  return result;
}

int extract_adc_poco(struct katcp_dispatch *d, struct katcp_sensor *sn)
{
  struct katcp_acquire *a;
  struct katcp_integer_acquire *ia;
  struct katcp_integer_sensor *is;
  int ant, pol, number;
  char buffer[3];

  if(sn->s_type != KATCP_SENSOR_BOOLEAN){
    return -1;
  }

  a = sn->s_acquire;
  if(a->a_type != KATCP_SENSOR_INTEGER){
    return -1;
  }

  is = sn->s_more;
  ia = a->a_more;

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "decoding raw sensor value 0x%04x", ia->ia_current);

  /* a bit of a hack - api needs an overhaul */
#ifdef DEBUG
  if(strlen(sn->s_name) != 22){
    fprintf(stderr, "logic problem: expected a string 22 characters long, not %s\n", sn->s_name);
    abort();
  }
#endif

  strncpy(buffer, sn->s_name + 11, 2);
  buffer[2] = '\0';

  ant = extract_ant_poco(buffer);
  pol = extract_polarisation_poco(buffer);

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "got board %d and connector %d from string %s", ant, pol, buffer);

  number = ant * 2 + pol;

  if(OVERRANGE_ADC_STATUS(0x1 << number) & ia->ia_current){
    is->is_current = 1;
    set_status_sensor_katcp(sn, KATCP_STATUS_WARN);
  } else {
    is->is_current = 0;
    set_status_sensor_katcp(sn, KATCP_STATUS_NOMINAL);
  }

  return 0;
}

int extract_quant_poco(struct katcp_dispatch *d, struct katcp_sensor *sn)
{
  /* TODO: should refactor extract_quant and extract_adc - too much duplication */
  struct katcp_acquire *a;
  struct katcp_integer_acquire *ia;
  struct katcp_integer_sensor *is;
  int ant, pol, number;
  char buffer[3];

  if(sn->s_type != KATCP_SENSOR_BOOLEAN){
    return -1;
  }

  a = sn->s_acquire;
  if(a->a_type != KATCP_SENSOR_INTEGER){
    return -1;
  }

  is = sn->s_more;
  ia = a->a_more;

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "decoding raw sensor value 0x%04x", ia->ia_current);

  /* a bit of a hack - api needs an overhaul */
#ifdef DEBUG
  if(strlen(sn->s_name) != 22){
    fprintf(stderr, "logic problem: expected a string 22 characters long, not %s\n", sn->s_name);
    abort();
  }
#endif

  strncpy(buffer, sn->s_name + 11, 2);
  buffer[2] = '\0';

  ant = extract_ant_poco(buffer);
  pol = extract_polarisation_poco(buffer);

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "got board %d and connector %d from string %s", ant, pol, buffer);

  number = ant * 2 + pol;

  if(OVERRANGE_QNT_STATUS(0x1 << number) & ia->ia_current){
    is->is_current = 1;
    set_status_sensor_katcp(sn, KATCP_STATUS_WARN);
  } else {
    is->is_current = 0;
    set_status_sensor_katcp(sn, KATCP_STATUS_NOMINAL);
  }

  return 0;
}

int extract_delay_poco(struct katcp_dispatch *d, struct katcp_sensor *sn)
{
  /* TODO: should refactor extract_quant, extract_armed and extract_adc - too much duplication */
  struct katcp_acquire *a;
  struct katcp_integer_acquire *ia;
  struct katcp_integer_sensor *is;
  int ant, pol, number;
  char buffer[3];

  if(sn->s_type != KATCP_SENSOR_BOOLEAN){
    return -1;
  }

  a = sn->s_acquire;
  if(a->a_type != KATCP_SENSOR_INTEGER){
    return -1;
  }

  is = sn->s_more;
  ia = a->a_more;

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "decoding raw sensor value 0x%04x", ia->ia_current);

  /* a bit of a hack - api needs an overhaul */
#ifdef DEBUG
  if(strlen(sn->s_name) != 19){
    fprintf(stderr, "logic problem: expected a string 22 characters long, not %s\n", sn->s_name);
    abort();
  }
#endif

  strncpy(buffer, sn->s_name + 11, 2);
  buffer[2] = '\0';

  ant = extract_ant_poco(buffer);
  pol = extract_polarisation_poco(buffer);

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "got board %d and connector %d from string %s", ant, pol, buffer);

  number = ant * 2 + pol;

  if(COARSE_DELAY_PENDING_STATUS(0x1 << number) & ia->ia_current){
    is->is_current = 1;
    set_status_sensor_katcp(sn, KATCP_STATUS_WARN);
  } else {
    is->is_current = 0;
    set_status_sensor_katcp(sn, KATCP_STATUS_NOMINAL);
  }

  return 0;
}
