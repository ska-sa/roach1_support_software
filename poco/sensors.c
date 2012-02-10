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

/* hwmon sensor stuff ************************************************/

static int file_number_hwmon(struct katcp_dispatch *d, int fd)
{
#define BUFFER 32
  char buffer[BUFFER];
  char *end;
  int rr, value;

  if(lseek(fd, 0, SEEK_SET) != 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to rewind fd %d", fd);
    return -1;
  }

  rr = read(fd, buffer, BUFFER - 1);
  if(rr <= 0){
    if(rr < 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "read of fd %d failed with error %s", fd, strerror(errno));
    } else {
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "fd %d not ready or empty", fd);
    }
    return -1;
  }

  buffer[rr] = '\0';

  value = strtol(buffer, &end, 0);

  switch(end[0]){
    case ' '  :
    case '\r' :
    case '\n' :
    case '\0' :
    break;
    default :
      log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "trailing data %s while extracting number from fd %d", end, fd);
    break;
  }

  return value;
#undef BUFFER
}

int acquire_hwmon_poco(struct katcp_dispatch *d, void *data)
{
  struct hwmon_sensor_poco *hp;
  int value;

  hp = data;

  value = file_number_hwmon(d, hp->h_fd);

  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "got %d to fall in range %d to %d", value, hp->h_least, hp->h_most);

  if((value < hp->h_least) || (value > hp->h_most)){
    /* value out of safe range, this is an error */
    return 0;
  }

  /* value in safe range */
  return 1;
}

int extract_hwmon_katcp(struct katcp_dispatch *d, struct katcp_sensor *sn)
{
  struct katcp_acquire *a;
  struct katcp_integer_acquire *ia;
  struct katcp_integer_sensor *is;

  a = sn->s_acquire;

  if((a == NULL) || (a->a_type != KATCP_SENSOR_BOOLEAN) || (sn->s_type != KATCP_SENSOR_BOOLEAN)){
    return -1;
  }

  is = sn->s_more;
  ia = a->a_more;

  if(ia->ia_current){
    set_status_sensor_katcp(sn, KATCP_STATUS_NOMINAL);
  } else {
    set_status_sensor_katcp(sn, KATCP_STATUS_ERROR);
  }

  is->is_current = ia->ia_current;

  return 0;
}

int setup_hwmon_poco(struct katcp_dispatch *d, struct hwmon_sensor_poco *hp, char *label, char *description, char *file, int least, int most)
{
  struct katcp_acquire *a;

  /* TODO: prolly ok for hwsensors, but for the general case this should be opened on demand, ie later and restart on failure */
  hp->h_fd = open(file, O_RDONLY);
  if(hp->h_fd < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to open %s: %s", file, strerror(errno));
    return -1;
  }

  fcntl(hp->h_fd, F_SETFD, FD_CLOEXEC);

  hp->h_least = least;
  hp->h_most = most;

  a = setup_boolean_acquire_katcp(d, &acquire_hwmon_poco, hp);
  if(a == NULL){
    close(hp->h_fd);
    hp->h_fd = (-1);
    return -1;
  }

  if(register_multi_boolean_sensor_katcp(d, POCO_POCO_MODE, label, description, "none", a, &extract_hwmon_katcp) < 0){
    destroy_acquire_katcp(d, a);
  }

  return 0;
}

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

  if(IS_COARSE_ARMED_STATUS(0x1 << number) & ia->ia_current){
    is->is_current = 1;
    set_status_sensor_katcp(sn, KATCP_STATUS_WARN);
  } else {
    is->is_current = 0;
    set_status_sensor_katcp(sn, KATCP_STATUS_NOMINAL);
  }

  return 0;
}
