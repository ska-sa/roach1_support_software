#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/utsname.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <katcp.h>
#include <katpriv.h>

#include "modes.h"

#define MTU 1500
int run_announce_katcp(struct katcp_dispatch *d, char *type, char *host, int port)
{
  int fd, option, wb, sp, i;
  socklen_t len;
  unsigned char buffer[MTU];
  struct sockaddr_in out, sa;
  struct utsname un;

#if 0
  char *sip;
#endif
  char *test[] = {"broadcast", "unicast", NULL};

  for (i=0; test[i] && (strcmp(test[i],type) != 0); i++);
  if (test[i] == NULL)
    return KATCP_RESULT_FAIL;

#ifdef DEBUG
  fprintf(stderr,"announce: type: [%d] %s\n", i, type);
#endif

  if (port <= 0)
    return KATCP_RESULT_FAIL;

  if (uname(&un) < 0){
#ifdef DEBUG
    fprintf(stderr,"announce: uname error %s\n", strerror(errno));
#endif
    return KATCP_RESULT_FAIL;
  }
  
  fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (fd < 0){
#ifdef DEBUG
    fprintf(stderr,"announce: socket error %s\n", strerror(errno));
#endif
    return KATCP_RESULT_FAIL;
  }

  out.sin_family      = AF_INET;
  out.sin_port        = htons(port);

  if (host == NULL) {
    out.sin_addr.s_addr = htonl(INADDR_BROADCAST);
  }
  else {
    if (inet_aton(host, &(out.sin_addr)) == 0){
#ifdef DEBUG
      fprintf(stderr,"announce: inet_aton error expected well formed ip not %s\n",host);
#endif
      return KATCP_RESULT_FAIL;  
    }
  }
 
  if (i == 0){
    option = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &option, sizeof(option)) < 0){
#ifdef DEBUG
      fprintf(stderr,"announce: setsockopt error cannot set SO_BROADCAST %s\n", strerror(errno));
#endif
      return KATCP_RESULT_FAIL;
    }
    else {
#ifdef DEBUG
      fprintf(stderr,"announce: set broadcast flag on fd\n");
#endif
    }
  }
#ifdef DEBUG
  fprintf(stderr,"announce: inet_ntoa %s\n", inet_ntoa(out.sin_addr));
#endif
  
  
  len = sizeof(struct sockaddr);
  if (getsockname(d->d_shared->s_lfd, (struct sockaddr *) &sa, &len) < 0){
#ifdef DEBUG
    fprintf(stderr,"announce: couldn't getsockname with server fd %s\n", strerror(errno));
#endif
    return KATCP_RESULT_FAIL;
  }
 
  sp = ntohs(sa.sin_port);
#if 0
  sip = inet_ntoa(sa.sin_addr);

  if (sprintf((char*)buffer,"#roach katcp://%s:%d/ %s", un.nodename, sp, sip) < 0){
#endif
  if (sprintf((char*)buffer,"katcp://%s:%d/", un.nodename, sp) < 0){
#ifdef DEBUG
    fprintf(stderr,"announce: sprintf error couldn't create string\n");
#endif
    return KATCP_RESULT_FAIL;
  }
 
#if 0
  if (connect(fd, (struct sockaddr *) &out, sizeof(struct sockaddr_in)) < 0){
#ifdef DEBUG
    fprintf(stderr,"announce: connect error %s\n",strerror(errno));
#endif
    return KATCP_RESULT_FAIL;
  }
#endif

#ifdef DEBUG
  fprintf(stderr,"announce: about to send: %s\n", buffer);
#endif

  wb = sendto(fd, buffer, strlen((char*)buffer), MSG_NOSIGNAL, (struct sockaddr *) &out, sizeof(struct sockaddr_in));
  
#if 0 
  wb = send(fd, buffer, strlen((char*)buffer), MSG_NOSIGNAL);
#endif

  if (wb < 0) {
#ifdef DEBUG
    fprintf(stderr,"announce: sendto error %s\n", strerror(errno));
#endif
    return KATCP_RESULT_FAIL;
  }
  
#ifdef DEBUG
  fprintf(stderr,"announce: wrote %d bytes via udp to broadcast\n", wb);
#endif

  return KATCP_RESULT_OK;
}
#undef MTU

int announce_cmd(struct katcp_dispatch *d, int argc)
{
  char *ptr, *host, *type;
  int port;

  port = 0;
  host = NULL;

  switch(argc){

    case 1:
      port = 7147;
      type = "broadcast";
      log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "announce to broadcast on 7147");
    case 4:

      if (port == 0){
        type = arg_string_katcp(d,1);
        host = arg_string_katcp(d,2);
        ptr  = arg_string_katcp(d,3);
        port = atoi(ptr);
      } 

      return run_announce_katcp(d, type, host, port);
  }
  
  return KATCP_RESULT_FAIL;
}

int setup_announce(struct katcp_dispatch *d)
{
  int result;

  result = 0;

  result += register_mode_katcp(d, "?announce", "start announcing to broadcast via udp (?announce [broadcast|unicast] [host] [port])", &announce_cmd, POCO_RAW_MODE);

  if (result < 0) {
#ifdef DEBUG
    fprintf(stderr, "announce: unable to register raw mode command");
#endif
    return -1;
  }

  return 0;
}
