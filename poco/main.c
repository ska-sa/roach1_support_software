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

#include <katcp.h>
#include <katsensor.h>
#include <katpriv.h>

#include "core.h"
#include "raw.h"
#include "poco.h"
#include "config.h"

void usage(char *app)
{
  printf("Usage: %s [-b bof-dir] [-m mode] [-p network-port] [-d fake-register-dir] [-i poco-image]\n", app);

  printf("-b bof-dir       directory to search for bof files in raw mode (default %s)\n", POCO_RAW_PATH);
  printf("-m mode          mode to enter at startup\n");
  printf("-p network-port  network port to listen on\n");
  printf("-i poco-image    bof file to program in poco mode (default %s)\n", POCO_RAW_PATH);
}

int main(int argc, char **argv)
{
  struct katcp_dispatch *d;
  int status;
  int i, j, c;
  char *bofs, *port, *fake, *mode, *image;

  image = POCO_POCO_IMAGE;
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
        case 'b' :
        case 'd' :
        case 'i' :
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

  /* load up build and version information */
  version_katcp(d, "poco", 0, 1);
  build_katcp(d, POCO_BUILD);

  if(setup_core_poco(d, fake) < 0){
    fprintf(stderr, "%s: unable to set up core logic\n", argv[0]);
  }

  if(setup_raw_poco(d, bofs, 1) < 0){
    fprintf(stderr, "%s: unable to set up raw logic\n", argv[0]);
  }

  if(setup_poco_poco(d, image) < 0){
    fprintf(stderr, "%s: unable to set up poco logic\n", argv[0]);
  }

  if(mode){
    if(enter_name_mode_katcp(d, mode) < 0){
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
