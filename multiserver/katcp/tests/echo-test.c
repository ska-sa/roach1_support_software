
#include <stdio.h>
#include <stdlib.h>

#include <katcp.h>

#ifndef BUILD
#define BUILD "unknown-0.0"
#endif

int echo_cmd(struct katcp_dispatch *d, int argc)
{
  int i;
  char *ptr, *tmp;
  int have, want, result;

  append_string_katcp(d, KATCP_FLAG_FIRST | ((argc <= 1) ? KATCP_FLAG_LAST : 0), "!echo");
  have = 0;
  ptr = NULL;

  i = 1;
  while(i < argc){
    want = arg_buffer_katcp(d, i, ptr, have);
#ifdef DEBUG
    fprintf(stderr, "echo: length of arg %d is %d\n", i, want);
#endif

    if(want > have){
      tmp = realloc(ptr, want);
      if(tmp == NULL){
        free(ptr);
        return KATCP_RESULT_OWN; /* problematic failure, broken reply */
      }
      ptr = tmp;
      have = want;
    } else {
      i++;
#ifdef DEBUG
      fprintf(stderr, "echo: appending %d\n", want);
#endif
      result = append_buffer_katcp(d, ((argc == i) ? KATCP_FLAG_LAST : 0), ptr, want);
      if(result < 0){
#ifdef DEBUG
        fprintf(stderr, "echo: append failed (code=%d)\n", result);
#endif
        free(ptr);
        return KATCP_RESULT_OWN;
      }
    }

  }

  if(ptr){
    free(ptr);
  }

  return KATCP_RESULT_OWN;
}

int main(int argc, char **argv)
{
  struct katcp_dispatch *d;
  int status;

  if(argc <= 1){
    fprintf(stderr, "usage: %s [bind-ip:]listen-port\n", argv[0]);
    return 1;
  }

  d = startup_katcp();
  if(d == NULL){
    fprintf(stderr, "%s: unable to allocate state\n", argv[0]);
    return 1;
  }

  version_katcp(d, "echo-test", 0, 1);
  build_katcp(d, BUILD);

  if(register_katcp(d, "?echo", "echo returns its parameters", &echo_cmd)){
    fprintf(stderr, "server: unable to enroll command\n");
    return 1;
  }

  if(run_server_katcp(d, argv[1], 0) < 0){
    fprintf(stderr, "server: run failed\n");
    return 1;
  }

  status = exited_katcp(d);

  shutdown_katcp(d);

  return status;
}
