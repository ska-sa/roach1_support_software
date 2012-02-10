/* a simple server example which registers a couple of sensors and commands */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <ctype.h>
#include <sysexits.h>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>

#include <katcp.h>
#include <katsensor.h>

/* build string **************************************************************/
/* when compiled with gcc BUILD can be set at compile time with -DBUILD=...  */

#ifndef BUILD
#define BUILD "unknown-0.0"
#endif

/* simple sensor functions ***************************************************/
/* these functions return the value immediately. This approach is acceptable */
/* when it is cheap to query a sensor value                                  */

int simple_integer_check_sensor(struct katcp_dispatch *d, void *local)
{
  return ((int)time(NULL)) / 10;
}

/* command functions *********************************************************/

/* check command 1: generates its own reply, with binary and integer output */

int check1_cmd(struct katcp_dispatch *d, int argc)
{
  send_katcp(d, KATCP_FLAG_FIRST | KATCP_FLAG_STRING, "!cmd-check-1", KATCP_FLAG_BUFFER, "\0\n\r ", 4, KATCP_FLAG_LAST | KATCP_FLAG_ULONG, 42UL);

  return KATCP_RESULT_OWN; /* we send our own return codes */
}

/* check command 2: has the infrastructure generate its reply */

int check2_cmd(struct katcp_dispatch *d, int argc)
{
  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "saw a check 2 message with %d arguments", argc);

  return KATCP_RESULT_OK; /* have the system send a status message for us */
}

int main(int argc, char **argv)
{
  struct katcp_dispatch *d;
  int status;

  if(argc <= 1){
    fprintf(stderr, "usage: %s [bind-ip:]listen-port\n", argv[0]);
    return 1;
  }

  /* create a state handle */
  d = startup_katcp();
  if(d == NULL){
    fprintf(stderr, "%s: unable to allocate state\n", argv[0]);
    return 1;
  }

  /* load up build and version information */
  version_katcp(d, "exampleserver", 0, 1);
  build_katcp(d, BUILD);

  /* register example sensor */

  if(register_integer_sensor_katcp(d, 0, "check.integer.simple", "unix time in decaseconds", "Ds", &simple_integer_check_sensor, NULL, 0, INT_MAX)){
    fprintf(stderr, "server: unable to register sensors\n");
    return 1;
  }

  /* register example commands */

  if(register_katcp(d, "?cmd-check-1", "test command 1", &check1_cmd)){
    fprintf(stderr, "server: unable to enroll command\n");
    return 1;
  }

  if(register_katcp(d, "?cmd-check-2", "test command 2 with log message", &check2_cmd)){
    fprintf(stderr, "server: unable to enroll command\n");
    return 1;
  }

#if 1
  /* alternative - run with more than one client */
  #define CLIENT_COUNT 3

  if(run_multi_server_katcp(d, CLIENT_COUNT, argv[1], 0) < 0){
    fprintf(stderr, "server: run failed\n");
  }
#else
  if(run_server_katcp(d, argv[1], 0) < 0){
    fprintf(stderr, "server: run failed\n");
  }
#endif

  status = exited_katcp(d);

  shutdown_katcp(d);

  return status;
}
