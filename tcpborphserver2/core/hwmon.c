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

#include "core.h"
#include "registers.h"
#include "modes.h"
#include "misc.h"
#include "input.h"

/* hwmon sensor stuff ************************************************/

#define FAN_WARN_LIMIT 1000

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

/************************************************************************************************/

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

#if 0
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
#endif

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

#if 0
  if(register_multi_boolean_sensor_katcp(d, POCO_POCO_MODE, label, description, "none", a, &extract_hwmon_katcp) < 0){
    destroy_acquire_katcp(d, a);
  }
#endif

  if(register_direct_multi_boolean_sensor_katcp(d, POCO_CORE_MODE, label, description, "none", a) < 0){
    destroy_acquire_katcp(d, a);
  }

  return 0;
}

