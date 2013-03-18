#include <stdio.h>

#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <dirent.h>
#include <sysexits.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdint.h>

#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <katcp.h>
#include <katsensor.h>
#include <katpriv.h>

#include <core.h>
#include "raw.h"
#include "modes.h"

#define CORE_MAGIC 0x1f1130c0

int programmed_core_poco(struct katcp_dispatch *d, char *image)
{
  struct poco_core_state *cs; 

  cs = get_mode_katcp(d, POCO_CORE_MODE);
  if(cs == NULL){
    return -1;
  }

  if(!cs->c_real){
    return (cs->c_size > 0) ? 1 : 0;
  }

  if(image){
    if(cs->c_borph_image == NULL){
      return 0;
    }

    return strcmp(image, cs->c_borph_image) ? 0 : 1;
  } else {
    /* WARNING: unchecked modification: is this really a reliable way to see if a bof file is running */
    return cs->c_borph_image ? 1 : 0;
  }
}

static void collect_core_poco(struct katcp_dispatch *d, int status)
{
  struct poco_core_state *cs; 

#ifdef DEBUG
  fprintf(stderr, "collect core: exit status is %d\n", status);
#endif

  log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "borph process exited");

  cs = get_mode_katcp(d, POCO_CORE_MODE);
  if(cs == NULL){
    return;
  }

  if(cs->c_borph_fd >= 0){
    close(cs->c_borph_fd);
    cs->c_borph_fd = (-1);
  }

#if 0 /* don't clear this, we use it to fake things */
  cs->c_borph_proc[0] = '\0';
#endif

  if(cs->c_borph_image){
    free(cs->c_borph_image);
    cs->c_borph_image = NULL;
  } else {
    log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "borph exited, but no image to clear");
  }

  /* TODO - maybe force a different mode */
}

int poll_program_core(struct katcp_dispatch *d, pid_t pid)
{
  time_t now, finish;
  uint32_t value;
  int got, fd;
  char buffer[POCO_CORE_BORPH];
  struct poco_core_state *cs; 
  struct stat st;

  cs = get_mode_katcp(d, POCO_CORE_MODE);
  if(cs == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to acquire core structure");
    return -1;
  }

  got = snprintf(buffer, POCO_CORE_BORPH, "/proc/%d/hw/status", pid);
  if((got < 0) || (got > POCO_CORE_BORPH)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "snprintf failed: have %d bytes, returned %d", POCO_CORE_BORPH, got);
    return -1;
  }

  now = time(NULL);
  finish = now + POCO_LOADTIME;

  while(stat(buffer, &st) != 0){
    /* Jason and Dave wanted aggresive polling ... */
    now = time(NULL);
    if(now > finish){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "timed out while waiting for fpga status to change: %s", strerror(errno));
      got = snprintf(buffer, POCO_CORE_BORPH, "/proc/%d/hw/ioreg", pid);
      if((got < 0) || (got > POCO_CORE_BORPH)){
        log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "snprintf failed: have %d bytes, returned %d", POCO_CORE_BORPH, got);
        return -1;
      }
      if(stat(buffer, &st) == 0){
        log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "timed out while waiting for status register, but returning ok because ioreg exists");
        return 0;
      }
      return -1;
    } else {
      sleep(1);
    }
  }

  fd = open(buffer, O_RDONLY);
  if(fd < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to open Dave's status register: %s", strerror(errno));
    return -1;
  }

  value = 0;

  do{

    if(lseek(fd, 0, SEEK_SET) != 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to seek to beginning of file");
      close(fd);
      return -1;
    }

    got = read(fd, buffer, 4);
    if(got == 4){ /* sometimes borph returns garbage */
      buffer[4] = '\0';
      value = atoi(buffer);
    }

    now = time(NULL);
    if(now > finish){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "Dave's status register still 0x%x after %ds, giving up", value, POCO_LOADTIME);
      close(fd);
      return -1;
    } else {
      sleep(1);
    }

  } while(!(value & 0x1));

  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "Dave's status register says 0x%x", value);

  close(fd);

  return 0;
}

pid_t program_core_poco(struct katcp_dispatch *d, char *image)
{
  DIR *dr;
  struct dirent *de;
  char *tmp;
  int pair[2];
  int i, len;
  pid_t pid;
  int status, result;

  struct poco_core_state *cs; 
  pid = 0;

  cs = get_mode_katcp(d, POCO_CORE_MODE);
  if(cs == NULL){
    return -1;
  }

  if(cs->c_size){
    log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "deallocating %d %s register entries", cs->c_size, cs->c_real ? "real" : "fake");
  }
  clear_all_pce(d); /* remove all entries */

  /* TODO: close any file descriptors before spawning child processes */

  if(cs->c_real){
    if(cs->c_borph_fd >= 0){
      log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "closing child pipe");
      close(cs->c_borph_fd);
      cs->c_borph_fd = (-1);
    }

#if 0 /* don't clear, used to fake registers */
    cs->c_borph_proc[0] = '\0';
#endif

    if(cs->c_borph_image){
      free(cs->c_borph_image);
      cs->c_borph_image = NULL;
    }

    if(end_name_shared_katcp(d, "borph", 1) < 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to terminate borph process");
      pid = (-1);
    }
  }

  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "system deprogrammed");

  if(image == NULL){
    return pid;
  }

  if(cs->c_real){
    log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "attempting to program %s", image);

    if(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) < 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to create socketpair: %s", strerror(errno));
      return -1;
    }

    pid = fork();
    if(pid < 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to spawn process: %s", strerror(errno));
      close(pair[0]);
      close(pair[1]);
      return -1;
    }

    if(pid == 0){
      /* in child, inadvisable to do io to network fd */

      close(pair[1]);
      for(i = STDIN_FILENO; i <= STDERR_FILENO; i++){
        if(i != pair[0]){
          dup2(pair[0], i);
        }
      }
      if(pair[0] >= i){
        close(pair[0]);
      }

      execl(image, image, NULL);

      /* TODO: release remaining fds with shutdown_katcp(d); */
      exit(EX_OSERR);
      /* not reached */
    }

#if 0
    flush_katcp(d); /* do some io before we stall */
#endif

    close(pair[0]);
    cs->c_borph_fd = pair[1];

    result = poll_program_core(d, pid);

    if(waitpid(pid, &status, WNOHANG) > 0){
      log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "collected process id %d with status 0x%x", pid, status);
      collect_core_poco(d, status);
      return -1;
    }

    len = snprintf(cs->c_borph_proc, POCO_CORE_BORPH, "/proc/%d/hw/ioreg", pid);
    if((len <= 0) || (len >= POCO_CORE_BORPH)){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "%d bytes insufficient for proc entry", POCO_CORE_BORPH - 1);
      result = (-1);
    } 
    cs->c_borph_proc[POCO_CORE_BORPH - 1] = '\0';

    if(watch_shared_katcp(d, "borph", pid, &collect_core_poco) < 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to monitor borph process");
      return -1;
    }

    if(result < 0){
      cs->c_borph_proc[0] = '\0';
      kill(pid, SIGTERM);
      return -1;
    }

  } else {
    log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "using fake registers from %s", cs->c_borph_proc);
    pid = getpid(); /* WARNING */
    if(cs->c_real){
      kill(pid, SIGTERM);
    }
  }

  cs->c_borph_image = strdup(image);
  if(cs->c_borph_image == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to duplicate string %s", image);
    if(cs->c_real){
      kill(pid, SIGTERM);
    }
    return -1;
  }

  dr = opendir(cs->c_borph_proc);
  if(dr == NULL){
    tmp = strerror(errno);
#ifdef DEBUG
    fprintf(stderr, "progdev: code %d maps to <%s>\n", errno, tmp);
#endif
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to open %s: %s", cs->c_borph_proc, tmp);
    if(cs->c_real){
      kill(pid, SIGTERM);
    }
    return -1;
  }

  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "opened directory %s", cs->c_borph_proc);

  while((de = readdir(dr))){
    if(de->d_name[0] != '.'){
#ifdef DEBUG
      fprintf(stderr, "progdev: attempting to acquire register %s\n", de->d_name);
#endif
      if(insert_pce(d, de->d_name)){
        log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "unable to add %s", de->d_name);
      } else {
        log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "registered %s", de->d_name);
      }
    } else {
      log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "not registering entry %s/%s", cs->c_borph_proc, de->d_name);
    }
  }

  closedir(dr);

  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "gathered %d entries", cs->c_size);

  return pid;
}

void destroy_core_poco(struct katcp_dispatch *d);

int setup_core_poco(struct katcp_dispatch *d, char *fake)
{
  struct poco_core_state *cs;
  unsigned int i;
  int result;
  
  if(fake){
    if(strlen(fake) >= (POCO_CORE_BORPH - 1)){
      return -1;
    }
  }

  cs = malloc(sizeof(struct poco_core_state));
  if(cs == NULL){
    return -1;
  }

  cs->c_magic = CORE_MAGIC;

  cs->c_borph_fd = (-1);
  cs->c_borph_image = NULL;

  cs->c_table = NULL;
  cs->c_size = 0;

  /* psu/system stuff */
  for(i = 0; i < POCO_HWMON_COUNT; i++){
    cs->c_hwmon_sensor[i].h_fd = (-1);
  }

  if(fake){
    strcpy(cs->c_borph_proc, fake);
    cs->c_real = 0;
  } else {
    cs->c_real = 1;
  }

  if(store_full_mode_katcp(d, POCO_CORE_MODE, POCO_CORE_NAME, NULL, NULL, cs, &destroy_core_poco) < 0){
#ifdef DEBUG
    fprintf(stderr, "poco: unable to store core state\n");
#endif
    return -1;
  }

  result = 0;

  result += setup_hwmon_poco(d, &(cs->c_hwmon_sensor[0]), "psu.3v3aux", "3.3v auxilliary powersupply good", "/sys/class/i2c-adapter/i2c-0/0-000f/power1_input", 1, 1);
  result += setup_hwmon_poco(d, &(cs->c_hwmon_sensor[1]), "psu.mgtpll", "MGTPLL powersupply good", "/sys/class/i2c-adapter/i2c-0/0-000f/power2_input", 1, 1);
  result += setup_hwmon_poco(d, &(cs->c_hwmon_sensor[2]), "psu.mgtvtt", "MGTVTT powersupply good", "/sys/class/i2c-adapter/i2c-0/0-000f/power3_input", 1, 1);
  result += setup_hwmon_poco(d, &(cs->c_hwmon_sensor[3]), "psu.mgtvcc", "MGTVCC powersupply good", "/sys/class/i2c-adapter/i2c-0/0-000f/power4_input", 1, 1);
  result += setup_hwmon_poco(d, &(cs->c_hwmon_sensor[4]), "fpga.fan",   "FPGA fan good", "/sys/class/i2c-adapter/i2c-0/0-000f/fan1_input", 150, 100000);

  if(result < 0){
    fprintf(stderr, "poco: unable to register hardware sensors\n");
  }

#ifdef DEBUG
  fprintf(stderr, "poco: added hwmon sensors\n");
#endif

  return 0;
}

void destroy_core_poco(struct katcp_dispatch *d)
{
  struct poco_core_state *cs;

  cs = get_mode_katcp(d, POCO_CORE_MODE);
  if(cs == NULL){
    return;
  }

  clear_all_pce(d);

  if(cs->c_borph_fd >= 0){
    close(cs->c_borph_fd);
    cs->c_borph_fd = (-1);
  }

  if(cs->c_borph_image){
    free(cs->c_borph_image);
    cs->c_borph_image = NULL;
  }

  end_name_shared_katcp(d, "borph", 0);

  cs->c_borph_proc[0] = '\0';

#if 0
  if(cs->c_borph_pid > 0){
    kill(cs->c_borph_pid, SIGKILL);
    cs->c_borph_pid = 0;
  }
#endif

  if(cs->c_table){
    free(cs->c_table);
    cs->c_table = NULL;
  }
  cs->c_size = 0;

  free(cs);
}
