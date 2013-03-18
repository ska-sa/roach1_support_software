#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <dirent.h>
#include <sysexits.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>

#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/utsname.h>

#include <katcp.h>
#include <katsensor.h>
#include <katpriv.h>

#include "core.h"
#include "raw.h"
#include "poco.h"
#include "config.h"
#include "holo.h"
#include "holo-config.h"
#include "announce.h"

void usage(char *app)
{
  printf("Usage: %s" 
#ifdef HAVE_MODE_RAW
  " [-b bof-dir]" 
#endif
#ifdef HAVE_MODE_POCO
  " [-i poco-image]"
#endif
#ifdef HAVE_MODE_HOLO
  " [-j holo-image]"
#endif
#ifdef DEBUG
  " [-d fake-register-dir]"
#endif
  " [-m mode] [-p network-port] [-d fake-register-dir]\n", app);

#ifdef HAVE_MODE_RAW
  printf("-b bof-dir       directory to search for bof files in raw mode (default %s)\n", POCO_RAW_PATH);
#endif
#ifdef HAVE_MODE_POCO
  printf("-i poco-image    bof file to program in poco mode (default %s)\n", POCO_RAW_PATH);
#endif
#ifdef HAVE_MODE_HOLO
  printf("-j holo-image    bof file to program in holo mode (default %s)\n", POCO_RAW_PATH);
#endif

  printf("-m mode          mode to enter at startup\n");
  printf("-p network-port  network port to listen on\n");

}

#define UNAME_BUFFER 128
int main(int argc, char **argv)
{
  struct katcp_dispatch *d;
  struct utsname un;
  int status;
  int i, j, c;
  char *bofs, *port, *fake, *mode, *image, *himage;
  char uname_buffer[UNAME_BUFFER];

#ifdef HAVE_MODE_POCO
  image = POCO_POCO_IMAGE;
#endif
  himage = POCO_HOLO_IMAGE;
  bofs = POCO_RAW_PATH;
  port = "7147";
  fake = NULL;
  mode = "raw";

  i = 1;
  j = 1;
  while (i < argc) {
    if (argv[i][0] == '-') {
      c = argv[i][j];
      switch (c) {
        case '\0':
          j = 1;
          i++;
          break;
        case '-':
          j++;
          break;
        case 'h' :
          usage(argv[0]);
          return EX_OK;
#if 0
        case 'q' :
          s->s_verbose = 0;
          j++;
          break;
        case 'v' :
          s->s_verbose++;
          j++;
          break;
#endif

#ifdef HAVE_MODE_RAW
        case 'b' :
#endif
#ifdef HAVE_MODE_POCO
        case 'i' :
#endif
#ifdef HAVE_MODE_HOLO
        case 'j' :
#endif
#ifdef DEBUG
        case 'd' :
#endif
        case 'm' :
        case 'p' :
          j++;
          if (argv[i][j] == '\0') {
            j = 0;
            i++;
          }
          if (i >= argc) {
            fprintf(stderr, "%s: option -%c requires a parameter\n", argv[0], c);
          }
          switch(c){
            case 'b' :
              bofs= argv[i] + j;
              break;
            case 'd' :
              fake = argv[i] + j;
              break;
            case 'i' :
              image = argv[i] + j;
              break;
            case 'j' :
              himage = argv[i] + j;
              break;
            case 'm' :
              mode = argv[i] + j;
              break;
            case 'p' :
              port = argv[i] + j;
              break;
          }
          i++;
          j = 1;
          break;
        default:
          fprintf(stderr, "%s: unknown option -%c\n", argv[0], c);
          return 1;
          break;
      }
    } else {
      fprintf(stderr, "%s: extra argument %s\n", argv[0], argv[i]);
      return 1;
    }
  }

  /* create a state handle */
  d = startup_katcp();
  if(d == NULL){
    fprintf(stderr, "%s: unable to allocate state\n", argv[0]);
    return 1;
  }

#ifdef TCPBORPHSERVER_BUILD
  add_build_katcp(d, TCPBORPHSERVER_BUILD);
#endif

  if(uname(&un) == 0){
    snprintf(uname_buffer, UNAME_BUFFER, "%s-%s", un.sysname, un.release);
    uname_buffer[UNAME_BUFFER - 1] = '\0';
#if 0
    add_build_katcp(d, uname_buffer);
#endif
  }

  if(setup_core_poco(d, fake) < 0){
    fprintf(stderr, "%s: unable to set up core logic\n", argv[0]);
  }

#ifdef HAVE_MODE_RAW
  if(setup_raw_poco(d, bofs, 1) < 0){
    fprintf(stderr, "%s: unable to set up raw logic\n", argv[0]);
  }
  if(setup_announce(d) < 0){
    fprintf(stderr,"%s: unable to set up announce logic\n", argv[0]);
  }
#endif

#ifdef HAVE_MODE_POCO
  if(setup_poco_poco(d, image) < 0){
    fprintf(stderr, "%s: unable to set up poco logic\n", argv[0]);
  }
#endif

#ifdef HAVE_MODE_HOLO
  if(setup_holo_poco(d, himage) < 0){
    fprintf(stderr, "%s: unable to set up holo logic\n", argv[0]);
  }
#endif

 

  if(mode){
    /* WARNING: API about to change, no longer returns mode code */
    if(enter_name_mode_katcp(d, mode, NULL) < 0){
      fprintf(stderr, "%s: unable to enter mode %s\n", argv[0], mode);
      return 1;
    }
  }

#if 0
  /* register example sensors */
  if(register_lru_sensor_katcp(d, "check.lru", "checks lru", "lru", KATCP_STRATEGY_EVENT, &lru_check_sensor)){
    fprintf(stderr, "server: unable to register lru sensor\n");
    return 1;
  }
#endif

#if 0
  if(register_katcp(d, "?mode", "test command 2 with log message", &check2_cmd)){
    fprintf(stderr, "server: unable to enroll command\n");
    return 1;
  }
#endif

  signal(SIGPIPE, SIG_DFL);

  if(run_multi_server_katcp(d, POCO_CLIENT_LIMIT, port, 0) < 0){
    fprintf(stderr, "server: run failed\n");
  }

  status = exited_katcp(d);

  shutdown_katcp(d);

#ifdef DEBUG
  fprintf(stderr, "server: exit with status %d\n", status);
#endif

  return status;
}
