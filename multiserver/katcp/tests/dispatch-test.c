#include <stdio.h>
#include <unistd.h>

#include <katcp.h>
#include <netc.h>

int main(int argc, char **argv)
{
  struct katcp_dispatch *d;
  int fd;

  fprintf(stderr, "dispatch.c test\n");

  if(argc > 1){
    fd = net_listen(argv[1], 0, 1);
    if(fd < 0){
      return 1;
    }
  } else {
    fd = STDIN_FILENO;
  }

  d = setup_katcp(fd);
  if(d == NULL){
    fprintf(stderr, "dispatch: start failed\n");
    return 1;
  }

  shutdown_katcp(d);

  return 0;
}

