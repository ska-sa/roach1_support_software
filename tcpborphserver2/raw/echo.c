#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <katcp.h>
#include <netc.h>

static void datamunge(unsigned char *buffer, unsigned int len, unsigned int base)
{
  int i;

  for(i = 0; i < len; i++){
    buffer[i] = (base + i) & 0xff;
  }
}

int echotest_cmd(struct katcp_dispatch *d, int argc)
{
  int fd, port, result, len, end;
  char *host;
  fd_set fsr, fsw;
  struct timeval timeout, start, stop;
  unsigned long delta; 
  unsigned int txl, rxl, count;
#define BUFFER     4096
#define ECHO_TIMEOUT 10
  unsigned char txb[BUFFER], rxb[BUFFER];

  if((argc <= 2) || arg_null_katcp(d, 1)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "insufficent arguments");
    return KATCP_RESULT_FAIL;
  }

  host = arg_string_katcp(d, 1);
  if(host == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "bad host");
    return KATCP_RESULT_FAIL;
  }

  port = arg_unsigned_long_katcp(d, 2);
  if((port == 0) ||  (port > 0xffff)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "need a decent port");
    return KATCP_RESULT_FAIL;
  }

  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "attemping to connect to %s:%d", host, port);

  fd = net_connect(host, port, 0);
  if(fd < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to connect: %s", strerror(errno));
    close(fd);
    return KATCP_RESULT_FAIL;
  }

  count = arg_unsigned_long_katcp(d, 3);
  log_message_katcp(d, KATCP_LEVEL_DEBUG, NULL, "will transmit %u bytes", count);

  txl = 0;
  rxl = 0;
  end = 0;

  FD_ZERO(&fsr);
  FD_ZERO(&fsw);

  gettimeofday(&start, NULL);

  while(rxl < count){
    timeout.tv_sec = ECHO_TIMEOUT;
    timeout.tv_usec = 0;

    FD_SET(fd, &fsr);
    if(end){
      FD_ZERO(&fsw);
    } else {
      FD_SET(fd, &fsw);
    }

    result = select(fd + 1, &fsr, end ? NULL : &fsw, NULL, &timeout);
    if(result < 0){
      switch(errno){
        case EAGAIN :
        case EINTR  :
          continue;
        default :
          log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "select failed: %s", strerror(errno));
          close(fd);
          return KATCP_RESULT_FAIL;
      }
    }

    if(result == 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "timeout after %u of %u bytes sent", txl, count);
      close(fd);
      return KATCP_RESULT_FAIL;
    }

    if(FD_ISSET(fd, &fsw)){
      len = (txl + BUFFER < count) ? BUFFER : (count - txl);
      datamunge(txb, len, txl);
      result = write(fd, txb, len);
      switch(result){
        case -1 :
          switch(errno){
            case EAGAIN :
            case EINTR  :
              break;
            default :
              log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "write failed: %s", strerror(errno));
              close(fd);
              return KATCP_RESULT_FAIL;
          }
          break;
        default :
          /* TODO: actually check content */
          txl += result;
          if(txl >= count){
            end = 1;
          }
          break;
      }
    }

    if(FD_ISSET(fd, &fsr)){
      result = read(fd, rxb, BUFFER);
      switch(result){
        case -1 :
          switch(errno){
            case EAGAIN :
            case EINTR  :
              break;
            default :
              log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "read failed: %s", strerror(errno));
              close(fd);
              return KATCP_RESULT_FAIL;
          }
          break;
        case  0 : 
          log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "premature end of file at %u", rxl);
          close(fd);
          return KATCP_RESULT_FAIL;
        default :
          /* TODO: actually check content */
          datamunge(txb, result, rxl);
          if(memcmp(txb, rxb, result)){
            log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "data mismatch after reading %u+%d", rxl, result);
            close(fd);
            return KATCP_RESULT_FAIL;
          }

          rxl += result;
          break;
      }
    }
  }

  gettimeofday(&stop, NULL);

  delta = stop.tv_sec - start.tv_sec;
  if(delta <= 0){
    delta = 1;
  }

  log_message_katcp(d, KATCP_LEVEL_INFO, NULL, "echotest start:%lu.%06u stop:%lu.%06u rate=%ub/s", start.tv_sec, start.tv_usec, stop.tv_sec, stop.tv_usec, count / delta);

  close(fd);
  return KATCP_RESULT_OK;
#undef BUFFER
}

