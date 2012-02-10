/* an attempt a a borph server which speaks the kat control protocol */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <sysexits.h>
#include <dirent.h> 

#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/time.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <katcp.h>
#include <netc.h>

#define MAX_CLIENTS 16

/* we implement the following revision of the protocol */
#define TCPBORPHSERVER_MAJOR 0
#define TCPBORPHSERVER_MINOR 1

#ifndef BUILD
#define BUILD "tcpborphserver-" SVNVERSION
#endif

#define DEFAULT_LISTCMD_PATH "/usr/bin" /* path to executable commands */
#define DEFAULT_BOF_PATH "/boffiles"    /* path to executable bof files */

#define LOADTIME 1                      /* wait XX seconds after loading a bitstream before trying to read the list of devices */

#define SUBSYSTEM "tcpborphserver"      /* name under which errors are reported */

#define CMDLENGTH 32

struct abs_entry{
	int e_fd;
	unsigned int e_size;
	char *e_name;
  int e_seek;
};

struct abs_state{
  char *s_bof_path;
  char *s_dev_path;
  int s_fpga_number;
  pid_t s_pid;

  struct abs_entry *s_table;
  int s_size;
  char *s_process;
  char *s_image;
  int s_pipe;
  int s_verbose;
};

static int insert_file(struct abs_state *s, char *name)
{
	struct stat st;
	int fd, len;
	struct abs_entry *tmp;
	char *ptr;

  tmp = realloc(s->s_table, sizeof(struct abs_entry) * (s->s_size + 1));
  if(tmp == NULL){
  	return -1;
  }

  s->s_table = tmp;
  tmp = &(s->s_table[s->s_size]);

  len = strlen(s->s_process) + strlen(name) + 2;
  ptr = malloc(len);
  if(ptr == NULL){
  	return -1;
  }

  snprintf(ptr, len, "%s/%s", s->s_process, name);
	fd = open(ptr, O_RDWR);
#ifdef DEBUG
  fprintf(stderr, "insert: open %s as %d\n", ptr, fd);
#endif
	free(ptr);

	if(fd >= 0){
    if(fstat(fd, &st) == 0){
      ptr = strdup(name);
      if(ptr){
        tmp->e_fd = fd;
        tmp->e_size = st.st_size;
        tmp->e_name = ptr;

        tmp->e_seek = S_ISFIFO(st.st_mode) ? 0 : 1;

        s->s_size++;
        return 0;
      }
    }
    close(fd);
  }

  return -1;
}

static int check_exit(struct katcp_dispatch *d)
{
  struct abs_state *s;
  pid_t collect;
  int status, result;

  s = get_state_katcp(d);
  result = 0;

  while((collect = waitpid(-1, &status, WNOHANG)) > 0){
  	if(collect == s->s_pid){
  		s->s_pid = (-1);
      result = 1;
    }
    if(WIFEXITED(status) && (WEXITSTATUS(status) == 0)){
      log_message_katcp(d, KATCP_LEVEL_INFO, SUBSYSTEM, "process id %d exited normally");
    } else {
      log_message_katcp(d, KATCP_LEVEL_WARN, SUBSYSTEM, "process id %d exited abnormally", collect);
    }
  }

  return result;
}

static int display_list(struct katcp_dispatch *d, char *path)
{
  DIR *dr;
  struct dirent *de;
  char *label;

  label = arg_string_katcp(d, 0);
  if(label == NULL){
    log_message_katcp(d, KATCP_LEVEL_FATAL, SUBSYSTEM, "internal logic failure");
  	return -1;
  }

  dr = opendir(path);
  if(dr == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "unable to open %s: %s", path, strerror(errno));
    return -1;
  }

  while((de = readdir(dr)) != NULL){
  	if(de->d_name[0] != '.'){ /* ignore hidden files */
      send_katcp(d,
        KATCP_FLAG_FIRST | KATCP_FLAG_STRING | KATCP_FLAG_MORE, "#", 
        KATCP_FLAG_STRING, label + 1,
        KATCP_FLAG_LAST | KATCP_FLAG_STRING, de->d_name);
    }
  }

  closedir(dr);

  /* TODO: possibly return the count with the ok message */

  return 0;
}

/********************************************************************/

static int datamunge(unsigned char *buffer, unsigned int len, unsigned int base)
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
#define BUFFER     1024
#define ECHO_TIMEOUT 10
  unsigned char txb[BUFFER], rxb[BUFFER];

  if((argc <= 2) || arg_null_katcp(d, 1)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "insufficent arguments");
    return KATCP_RESULT_FAIL;
  }

  host = arg_string_katcp(d, 1);
  if(host == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "bad host");
    return KATCP_RESULT_FAIL;
  }

  port = arg_unsigned_long_katcp(d, 2);
  if((port == 0) ||  (port > 0xffff)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "need a decent port");
    return KATCP_RESULT_FAIL;
  }

  log_message_katcp(d, KATCP_LEVEL_DEBUG, SUBSYSTEM, "attemping to connect to %s:%d", host, port);

  fd = net_connect(host, port, 0);
  if(fd < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "unable to connect: %s", strerror(errno));
    close(fd);
    return KATCP_RESULT_FAIL;
  }

  count = arg_unsigned_long_katcp(d, 3);
  log_message_katcp(d, KATCP_LEVEL_DEBUG, SUBSYSTEM, "will transmit %u bytes", count);

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
          log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "select failed: %s\n", strerror(errno));
          close(fd);
          return KATCP_RESULT_FAIL;
      }
    }

    if(result == 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "timeout after %u of %u bytes sent\n", txl, count);
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
              log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "write failed: %s\n", strerror(errno));
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
              log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "read failed: %s\n", strerror(errno));
              close(fd);
              return KATCP_RESULT_FAIL;
          }
          break;
        case  0 : 
          log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "premature end of file at %u", rxl);
          close(fd);
          return KATCP_RESULT_FAIL;
        default :
          /* TODO: actually check content */
          datamunge(txb, result, rxl);
          if(memcmp(txb, rxb, result)){
            log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "data mismatch after reading %u+%d", rxl, result);
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

  log_message_katcp(d, KATCP_LEVEL_INFO, SUBSYSTEM, "echotest start:%lu.%06u stop:%lu.%06u rate=%ub/s", start.tv_sec, start.tv_usec, stop.tv_sec, stop.tv_usec, count / delta);

  close(fd);
  return KATCP_RESULT_OK;
#undef BUFFER
}

/********************************************************************/

int progdev_cmd(struct katcp_dispatch *d, int argc)
{
  DIR *dr;
  struct dirent *de;
  struct abs_state *s;
  struct abs_entry *e;
  char *image, *tmp;
  int pair[2];
  int i;

  s = get_state_katcp(d);

  if(s->s_size > 0){
    for(i = 0; i < s->s_size; i++){
      e = &(s->s_table[i]);

      log_message_katcp(d, KATCP_LEVEL_DEBUG, SUBSYSTEM, "closing[%d]=%s", i, e->e_name);

      free(e->e_name);
      e->e_name = NULL;

      close(e->e_fd);
      e->e_fd = (-1);
    }
    s->s_size = 0;
  }

  if(s->s_pipe >= 0){
    log_message_katcp(d, KATCP_LEVEL_DEBUG, SUBSYSTEM, "closing child pipe");
  	close(s->s_pipe);
  	s->s_pipe = (-1);
  }

  check_exit(d);

  if(s->s_pid > 0){
    if(kill(s->s_pid, SIGKILL)){
    	if(errno == ESRCH){ /* already gone */
        log_message_katcp(d, KATCP_LEVEL_WARN, SUBSYSTEM, "process id %d already gone", s->s_pid);
      } else {
        log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "unable to end process id %d", s->s_pid);
        return KATCP_RESULT_FAIL;
      }
    } else {
      log_message_katcp(d, KATCP_LEVEL_INFO, SUBSYSTEM, "killed process id %d", s->s_pid);
    }
    s->s_pid = (-1);
  }

  /* no arguments seems to indicate just to stop a process */
  if((argc <= 1) || arg_null_katcp(d, 1)){
    return KATCP_RESULT_OK;
  }

  image = arg_string_katcp(d, 1);
  if(image == NULL){
    log_message_katcp(d, KATCP_LEVEL_FATAL, SUBSYSTEM, "unable to acquire device argument (arg count %d)", argc);
    return KATCP_RESULT_FAIL;
  }

  i = log_message_katcp(d, KATCP_LEVEL_DEBUG, SUBSYSTEM, "attempting to program %s", image);
#ifdef DEBUG
	fprintf(stderr, "log message returns %d\n", i);
#endif

  if(strchr(image, '/')){
    log_message_katcp(d, KATCP_LEVEL_FATAL, SUBSYSTEM, "client attempts to specify a path (%s)", image);
    return KATCP_RESULT_INVALID;
  }

  i = strlen(s->s_bof_path) + strlen(image) + 2;

  tmp = realloc(s->s_image, i);
  if(tmp == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "unable to allocate %d bytes", i);
  	return KATCP_RESULT_FAIL;
  }

  s->s_image = tmp;
  snprintf(s->s_image, i, "%s/%s", s->s_bof_path, image);

  if(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "unable to create socketpair: %s", strerror(errno));
    return KATCP_RESULT_FAIL;
  }

  s->s_pid = fork();
  if(s->s_pid < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "unable to spawn process: %s", strerror(errno));
    return KATCP_RESULT_FAIL;
  }

  if(s->s_pid == 0){
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

    execl(s->s_image, image, NULL);

    /* TODO: release remaining fds with shutdown_katcp(d); */
  	exit(EX_OSERR);
  	/* not reached */
  }

  flush_katcp(d); /* do some io before we stall */
  sleep(LOADTIME); /* WARNING, stalls everything */

  check_exit(d);
  if(s->s_pid < 0){
    return KATCP_RESULT_FAIL;
  }

  /* TODO: wait for child process */
  close(pair[0]);
  s->s_pipe = pair[1];

  if(s->s_dev_path){
  	if(s->s_process){
  		free(s->s_process);
    }
    s->s_process = strdup(s->s_dev_path);
  } else {
#define PROC_ENTRY_SIZE 64
  	tmp = realloc(s->s_process, PROC_ENTRY_SIZE);
  	if(tmp == NULL){
  		free(s->s_process);
  		s->s_process = NULL;
    } else {
    	s->s_process = tmp;
      snprintf(s->s_process, PROC_ENTRY_SIZE - 1, "/proc/%d/hw/ioreg", s->s_pid);
    }
#undef PROC_ENTRY_SIZE
  }

  if(s->s_process == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "unable to allocate memory for directory path");
  	return KATCP_RESULT_FAIL;
  }

  dr = opendir(s->s_process);
  if(dr == NULL){
  	tmp = strerror(errno);
#ifdef DEBUG
  	fprintf(stderr, "progdev: code %d maps to <%s>\n", errno, tmp);
#endif
    log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "unable to open %s: %s", s->s_process, tmp);
    /* TODO: kill process ? empty out process name ? */
    return KATCP_RESULT_FAIL;
  }

  log_message_katcp(d, KATCP_LEVEL_DEBUG, SUBSYSTEM, "opened directory %s", s->s_process);

  while((de = readdir(dr))){
  	if(de->d_name[0] != '.'){
  		if(insert_file(s, de->d_name)){
        log_message_katcp(d, KATCP_LEVEL_WARN, SUBSYSTEM, "unable to add %s", de->d_name);
      } else {
        log_message_katcp(d, KATCP_LEVEL_TRACE, SUBSYSTEM, "registered %s", de->d_name);
      }
    }
  }

  closedir(dr);

  log_message_katcp(d, KATCP_LEVEL_DEBUG, SUBSYSTEM, "gathered %d entries", s->s_size);

  return KATCP_RESULT_OK;
}

int status_cmd(struct katcp_dispatch *d, int argc)
{
  struct abs_state *s;

  check_exit(d);

  s = get_state_katcp(d);

  if((s->s_pid > 0) && (s->s_process)){

#ifdef DEBUG
    fprintf(stderr, "status: can report %s\n", s->s_process);
#endif

    send_katcp(d, KATCP_FLAG_FIRST | KATCP_FLAG_STRING, "!status", KATCP_FLAG_STRING, KATCP_OK, KATCP_FLAG_LAST | KATCP_FLAG_STRING, s->s_process);
  } else {
    send_katcp(d, KATCP_FLAG_FIRST | KATCP_FLAG_STRING, "!status", KATCP_FLAG_STRING, KATCP_FAIL, KATCP_FLAG_LAST | KATCP_FLAG_STRING, "no process running");
  }

  return KATCP_RESULT_OWN;
}

int listbof_cmd(struct katcp_dispatch *d, int argc)
{
  struct abs_state *s;

  s = get_state_katcp(d);
  if(s == NULL){
  	return KATCP_RESULT_FAIL;
  }

	return display_list(d, s->s_bof_path);
}

int listcmd_cmd(struct katcp_dispatch *d, int argc)
{
  struct abs_state *s;

  s = get_state_katcp(d);
  if(s == NULL){
  	return KATCP_RESULT_FAIL;
  }

  log_message_katcp(d, KATCP_LEVEL_WARN, SUBSYSTEM, "no exec facility currently available");

	return display_list(d, DEFAULT_LISTCMD_PATH);
}

int listdev_cmd(struct katcp_dispatch *d, int argc)
{
  struct abs_state *s;
  char *label;
  int i;

  s = get_state_katcp(d);
  if((s == NULL) || (s->s_pid <= 0)){
    log_message_katcp(d, KATCP_LEVEL_WARN, SUBSYSTEM, "no process running");

    send_katcp(d, KATCP_FLAG_FIRST | KATCP_FLAG_STRING, "!listdev", KATCP_FLAG_STRING, KATCP_FAIL, KATCP_FLAG_LAST | KATCP_FLAG_STRING, "no process running");
  	return KATCP_RESULT_OWN;
  }

  label = arg_string_katcp(d, 0);
  if(label == NULL){
    log_message_katcp(d, KATCP_LEVEL_FATAL, SUBSYSTEM, "internal logic failure");

    send_katcp(d, KATCP_FLAG_FIRST | KATCP_FLAG_STRING, "!listdev", KATCP_FLAG_STRING, KATCP_FAIL, KATCP_FLAG_LAST | KATCP_FLAG_STRING, "internal failure");
  	return KATCP_RESULT_OWN;
  }

  for(i = 0; i < s->s_size; i++){
    send_katcp(d,
        KATCP_FLAG_FIRST | KATCP_FLAG_STRING | KATCP_FLAG_MORE, "#", 
        KATCP_FLAG_STRING, label + 1,
        KATCP_FLAG_LAST | KATCP_FLAG_STRING, s->s_table[i].e_name);
  }
  
  /* TODO: possibly return the count with the ok message */

  return KATCP_RESULT_OK;
}

static int do_read_cmd(struct katcp_dispatch *d, int argc, int symbolic, int format)
{
	char *name;
	unsigned int start, length;
	unsigned long value;
  struct abs_state *s;
  int index, rr, have, fd, i, flags;
  char *ptr, reply[CMDLENGTH];

  s = get_state_katcp(d);
  if(s == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "unable to acquire state");
  	return KATCP_RESULT_FAIL;
  }

	if(argc <= 1){
    log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "need a register to read, followed by optional offset and count");
		return KATCP_RESULT_INVALID;
  }

  ptr = arg_string_katcp(d, 0);
  if(ptr == NULL){
    log_message_katcp(d, KATCP_LEVEL_FATAL, SUBSYSTEM, "internal logic failure");
  	return KATCP_RESULT_FAIL;
  }
  strncpy(reply, ptr, CMDLENGTH);
  reply[0] = KATCP_REPLY;

  length = 1;
  start = 0;

  if(symbolic){
    name = arg_string_katcp(d, 1);
    if(name == NULL){
      log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "no name available");
      return KATCP_RESULT_FAIL;
    }
    for(index = 0; (index < s->s_size) && strcmp(name, s->s_table[index].e_name); index++);
    if(index >= s->s_size){
      log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "%s unknown", name);

      send_katcp(d, KATCP_FLAG_FIRST | KATCP_FLAG_STRING, reply, KATCP_FLAG_STRING, KATCP_INVALID, KATCP_FLAG_STRING, "name", KATCP_FLAG_LAST | KATCP_FLAG_STRING, name);
      return KATCP_RESULT_OWN;
    }
  } else {
  	index = arg_unsigned_long_katcp(d, 1);
    if(index >= s->s_size){
      log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "%u out of range", index);
      return KATCP_RESULT_INVALID;
    }
    name = s->s_table[index].e_name;
  }

  if(argc > 2){
  	start = arg_unsigned_long_katcp(d, 2);
  }

  if(argc > 3){
  	length = arg_unsigned_long_katcp(d, 3);
  }

  if(format > 1){
  	length *= format;
  	start  *= format;
  }

  if(length <= 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "length has to be nonzero");
    return KATCP_RESULT_INVALID;
  }

#if 0
  base = strlen(s->s_process) + 1;
#endif

  log_message_katcp(d, KATCP_LEVEL_DEBUG, SUBSYSTEM, "reading %u from %s at %u", length, name, start);

  fd = s->s_table[index].e_fd;
  if(s->s_table[index].e_seek && (start + length > s->s_table[index].e_size)){
    log_message_katcp(d, KATCP_LEVEL_WARN, SUBSYSTEM, "range check fails in %s: %u+%u>%u", name, start, length, s->s_table[index].e_size);

    send_katcp(d, KATCP_FLAG_FIRST | KATCP_FLAG_STRING, reply, KATCP_FLAG_STRING, KATCP_FAIL, KATCP_FLAG_STRING, "range", KATCP_FLAG_LAST | KATCP_FLAG_STRING, name);
    return KATCP_RESULT_OWN;
  }

  if(s->s_table[index].e_seek == 0){
    if(start != 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "can not seek on stream %s", name);
      send_katcp(d, KATCP_FLAG_FIRST | KATCP_FLAG_STRING, reply, KATCP_FLAG_STRING, KATCP_FAIL, KATCP_FLAG_STRING, "seek", KATCP_FLAG_LAST | KATCP_FLAG_STRING, name);
      return KATCP_RESULT_OWN;
    } else {
      lseek(fd, 0, SEEK_SET);
    }
  } else if(s->s_table[index].e_seek && lseek(fd, start, SEEK_SET) != start){
    log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "seek to %u in %s failed: %s", start, name, strerror(errno));

    send_katcp(d, KATCP_FLAG_FIRST | KATCP_FLAG_STRING, reply, KATCP_FLAG_STRING, KATCP_FAIL, KATCP_FLAG_STRING, "seek", KATCP_FLAG_LAST | KATCP_FLAG_STRING, name);
    return KATCP_RESULT_OWN;
  }

  ptr = malloc(length);
  if(ptr == NULL){
    send_katcp(d, KATCP_FLAG_FIRST | KATCP_FLAG_STRING, reply, KATCP_FLAG_STRING, KATCP_FAIL, KATCP_FLAG_STRING, "malloc", KATCP_FLAG_LAST | KATCP_FLAG_STRING, name);
    return KATCP_RESULT_OWN;
  }

  have = 0;
  do{
  	rr = read(fd, ptr + have, length - have);

    log_message_katcp(d, KATCP_LEVEL_DEBUG, SUBSYSTEM, "read from %s: result=%d, wanted=%d, had=%d", name, rr, length - have, have);

    switch(rr){
      case -1 :
        switch(errno){
          case EAGAIN :
          case EINTR  : 
            rr = 0; /* not a critical problem */
            break;
          default :
            log_message_katcp(d, KATCP_LEVEL_WARN, SUBSYSTEM, "read failure on %s: %s\n", name, strerror(errno));
            break;
        }
        break;
      case  0 :
      default :
        have += rr;
      break;
    }
  } while((have < length) && (rr > 0));

  if(rr < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "read in %s failed: %s", name, strerror(errno));
  	free(ptr);

    send_katcp(d, KATCP_FLAG_FIRST | KATCP_FLAG_STRING, reply, KATCP_FLAG_STRING, KATCP_FAIL, KATCP_FLAG_STRING, "read", KATCP_FLAG_LAST | KATCP_FLAG_STRING, name);
    return KATCP_RESULT_OWN;
  }

  length = have; /* truncate to what we actually could get */

  append_string_katcp(d, KATCP_FLAG_FIRST | KATCP_FLAG_STRING, reply);
  append_string_katcp(d, KATCP_FLAG_STRING, KATCP_OK);

  switch(format){
    case 1 :
      i = 0; 
      flags = KATCP_FLAG_XLONG;
      while(i < length){
      	value = ptr[i];
      	i++;
      	if(i >= length){
      		flags |= KATCP_FLAG_LAST;
        }
        append_hex_long_katcp(d, flags, value);
      }
      break;
    case sizeof(unsigned int) :
      i = 0; 
      flags = KATCP_FLAG_XLONG;
      while(i < length){
      	/* WARNING: buffer should be aligned */
      	value = *((unsigned int *)(ptr + i));
      	i += sizeof(unsigned int);
      	if(i >= length){
      		flags |= KATCP_FLAG_LAST;
        }
        append_hex_long_katcp(d, flags, value);
      }
      break;
    default :
      append_buffer_katcp(d, KATCP_FLAG_LAST | KATCP_FLAG_BUFFER, ptr, length);
      break;
  }

  free(ptr);

  return KATCP_RESULT_OWN;
}

int indexread_cmd(struct katcp_dispatch *d, int argc)
{
	return do_read_cmd(d, argc, 0, 0);
}

int read_cmd(struct katcp_dispatch *d, int argc)
{
	return do_read_cmd(d, argc, 1, 0);
}

int wordread_cmd(struct katcp_dispatch *d, int argc)
{
	return do_read_cmd(d, argc, 1, 4);
}

static int do_write_cmd(struct katcp_dispatch *d, int argc, int symbolic, int format)
{
	char *name;
  unsigned char ucvalue;
	unsigned int start, length, total, uivalue;
  struct abs_state *s;
  int index, wr, fd, j, i;
  char *ptr, reply[CMDLENGTH];
  struct stat st;

  s = get_state_katcp(d);
  if(s == NULL){
  	return KATCP_RESULT_FAIL;
  }

	if(argc <= 3){
    log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "need a register, offset and data to write");
		return KATCP_RESULT_INVALID;
  }

  ptr = arg_string_katcp(d, 0);
  if(ptr == NULL){
    log_message_katcp(d, KATCP_LEVEL_FATAL, SUBSYSTEM, "internal logic failure");
		return KATCP_RESULT_FAIL;
  }
  strncpy(reply, ptr, CMDLENGTH);
  reply[0] = KATCP_REPLY;

  length = 1;
  start = 0;

  if(symbolic){
    name = arg_string_katcp(d, 1);
    if(name == NULL){
      log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "no name available");
		  return KATCP_RESULT_FAIL;
    }
    for(index = 0; (index < s->s_size) && strcmp(name, s->s_table[index].e_name); index++);
    if(index >= s->s_size){
      log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "%s unknown", name);

      send_katcp(d, KATCP_FLAG_FIRST | KATCP_FLAG_STRING, reply, KATCP_FLAG_STRING, KATCP_INVALID, KATCP_FLAG_STRING, "name", KATCP_FLAG_LAST | KATCP_FLAG_STRING, name);
      return KATCP_RESULT_OWN;
    }
  } else {
  	index = arg_unsigned_long_katcp(d, 1);
    if(index >= s->s_size){
      log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "%u out of range", index);
		  return KATCP_RESULT_INVALID;
    }
    name = s->s_table[index].e_name;
  }

  fd = s->s_table[index].e_fd;

  start = arg_unsigned_long_katcp(d, 2);
  if(format > 1){
  	start *= format;
  }
  if(s->s_table[index].e_seek && (lseek(fd, start, SEEK_SET) != start)){
    log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "seek to %u in %s failed", start, name);

    send_katcp(d, KATCP_FLAG_FIRST | KATCP_FLAG_STRING, reply, KATCP_FLAG_STRING, KATCP_FAIL, KATCP_FLAG_STRING, "seek", KATCP_FLAG_LAST | KATCP_FLAG_STRING, name);
    return KATCP_RESULT_OWN;
  }

  total = 0;

  for(i = 3; i < argc; i++){
    if(arg_null_katcp(d, i)){
      log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "empty data field");

      send_katcp(d, KATCP_FLAG_FIRST | KATCP_FLAG_STRING, reply, KATCP_FLAG_STRING, KATCP_FAIL, KATCP_FLAG_STRING, "null", KATCP_FLAG_LAST | KATCP_FLAG_STRING, name);
      return KATCP_RESULT_OWN;
    }

    switch(format){
      case 1 :
        ucvalue = arg_unsigned_long_katcp(d, i);
        wr = write(fd, &ucvalue, 1);
        if(wr != 1){
          log_message_katcp(d, KATCP_LEVEL_WARN, SUBSYSTEM, "unable to write byte to %s: %s", name, strerror(errno));

          send_katcp(d, KATCP_FLAG_FIRST | KATCP_FLAG_STRING, reply, KATCP_FLAG_STRING, KATCP_FAIL, KATCP_FLAG_STRING, "write", KATCP_FLAG_LAST | KATCP_FLAG_STRING, name);
          return KATCP_RESULT_OWN;
        }
        break;
      case sizeof(unsigned int) :
        uivalue = arg_unsigned_long_katcp(d, i);
        wr = write(fd, &uivalue, sizeof(unsigned int));
        if(wr != sizeof(unsigned int)){
          return KATCP_RESULT_OWN;
          log_message_katcp(d, KATCP_LEVEL_WARN, SUBSYSTEM, "unable to write %d bytes to %s: %s", sizeof(unsigned int), name, strerror(errno));

          send_katcp(d, KATCP_FLAG_FIRST | KATCP_FLAG_STRING, reply, KATCP_FLAG_STRING, KATCP_FAIL, KATCP_FLAG_STRING, "write", KATCP_FLAG_LAST | KATCP_FLAG_STRING, name);
          return KATCP_RESULT_OWN;
        }
        total += wr;
        break;
      default :

        length = arg_buffer_katcp(d, i, NULL, 0);
        /* WARNING: being too chummy with API internals */
        ptr = arg_string_katcp(d, i);

        if((length <= 0) || (ptr == NULL)){
          log_message_katcp(d, KATCP_LEVEL_FATAL, SUBSYSTEM, "internal problems, unable to access argument %d", i);
          return KATCP_RESULT_FAIL;
        }
        j = 0;
        while(j < length){
          wr = write(fd, ptr + j, length - j);
          if(wr <= 0){
            log_message_katcp(d, KATCP_LEVEL_WARN, SUBSYSTEM, "unable to write %d bytes at %d to %s: %s", length - j, start + j, name, strerror(errno));

            send_katcp(d, KATCP_FLAG_FIRST | KATCP_FLAG_STRING, reply, KATCP_FLAG_STRING, KATCP_FAIL, KATCP_FLAG_STRING, "write", KATCP_FLAG_LAST | KATCP_FLAG_STRING, name);
            return KATCP_RESULT_OWN;
          } 
          j += wr;
          total += wr;
        }
        break;
    }
  }

  log_message_katcp(d, KATCP_LEVEL_DEBUG, SUBSYSTEM, "wrote %u bytes to %s", total, name);

  if(fstat(fd, &st)){
    log_message_katcp(d, KATCP_LEVEL_WARN, SUBSYSTEM, "unable to do final stat: %s", strerror(errno));
  }

  if(s->s_table[index].e_size != st.st_size){
    log_message_katcp(d, KATCP_LEVEL_INFO, SUBSYSTEM, "file size updated to %u", (unsigned int) st.st_size); 
  }

  s->s_table[index].e_size = st.st_size;

  return KATCP_RESULT_OK;
}

int indexwrite_cmd(struct katcp_dispatch *d, int argc)
{
	return do_write_cmd(d, argc, 0, 0);
}

int write_cmd(struct katcp_dispatch *d, int argc)
{
	return do_write_cmd(d, argc, 1, 0);
}

int wordwrite_cmd(struct katcp_dispatch *d, int argc)
{
	return do_write_cmd(d, argc, 1, 4);
}

#if 0
int test_cmd(struct katcp_dispatch *d, int argc)
{

  send_katcp(d, KATCP_FLAG_FIRST | KATCP_FLAG_STRING, "!test", KATCP_FLAG_BUFFER, "\0\n\r ", 4, KATCP_FLAG_LAST | KATCP_FLAG_ULONG, 42UL);

  return KATCP_RESULT_OWN;
}
#endif

int do_startup(struct katcp_dispatch *d, struct abs_state *s)
{
  int result;

  version_katcp(d, SUBSYSTEM, TCPBORPHSERVER_MINOR, TCPBORPHSERVER_MAJOR);
  build_katcp(d, BUILD);
  set_state_katcp(d, s);

  result = 0;

  result += register_katcp(d, "?progdev",  "programs an image", &progdev_cmd);
  result += register_katcp(d, "?status",   "displays image status information", &status_cmd);
  result += register_katcp(d, "?listbof",  "displays available images", &listbof_cmd);
  result += register_katcp(d, "?listcmd",  "displays available shell commands", &listcmd_cmd);
  result += register_katcp(d, "?listdev",  "displays available device registers", &listdev_cmd);

  result += register_katcp(d, "?read",     "reads arg3 bytes starting at arg2 offset from register arg1", &read_cmd);
  result += register_katcp(d, "?indexread","reads arbitrary data lengths from a numbered register", &indexread_cmd);
  result += register_katcp(d, "?wordread", "reads data words from named a register", &wordread_cmd);

  result += register_katcp(d, "?write",     "writes arbitrary data lengths to a named register", &write_cmd);
  result += register_katcp(d, "?indexwrite","writes arbitrary data lengths to a numbered register", &indexwrite_cmd);
  result += register_katcp(d, "?wordwrite", "writes data words to a named register", &wordwrite_cmd);

  result += register_katcp(d, "?echotest", "basic network echo tester", &echotest_cmd);

  if(result){
    fprintf(stderr, "unable to enroll commands\n");
    return -1;
  }

  return 0;
}

#define NTOA_BUFFER 32

int main(int argc, char **argv)
{
  struct katcp_dispatch *clients[MAX_CLIENTS];
  char buffer[NTOA_BUFFER], *ptr;
  int lfd, tfd, run, j, i, used, max, add, status, c, result, detach;
  unsigned int len;
  fd_set fsr, fsw;
  char *app;
  struct timeval tv;
  struct abs_state state, *s;
  struct sockaddr_in local, remote;
  char *port;
  pid_t pid;

  detach = 0;

  port = "7147";
  s = &state;

  s->s_bof_path = DEFAULT_BOF_PATH;
  s->s_dev_path = NULL;
  s->s_fpga_number = 0;
  s->s_pid = (-1);

  s->s_table = NULL;
  s->s_size = 0;

  s->s_process = NULL;
  s->s_image = NULL;
  s->s_pipe = (-1);
  s->s_verbose = 1;

  app = argv[0];

  i = j = 1;
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
          printf("Usage: %s [-b bof path] [-f FPGA number] [-p port] [-d register path] [-q] [-v] [-z]\n", argv[0]);
          printf("  -d will fake registers from a specified location\n");
          return 0;
          break;
        case 'z' :
          detach = 1 - detach;
          j++;
          break;
        case 'q' :
          s->s_verbose = 0;
          j++;
          break;
        case 'v' :
          s->s_verbose++;
          j++;
          break;
        case 'b' :
        case 'd' :
        case 'f' :
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
              s->s_bof_path = argv[i] + j;
              break;
            case 'd' :
              s->s_dev_path = argv[i] + j;
              break;
            case 'f' :
              s->s_fpga_number = atoi(argv[i] + j);
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

  if(detach){
    pid = fork();
    if(pid < 0){
      fprintf(stderr, "%s: unable to fork: %s\n", argv[0], strerror(errno));
      return 1;
    }

    if(pid > 0){
      sleep(1); /* retarded */

      if(waitpid(pid, &status, WNOHANG) > 0){
        fprintf(stderr, "%s: unable to initialise child process\n", argv[0]);
      }

      return 0;
    }

    setsid();
  }

  for(i = 0; i < MAX_CLIENTS; i++){
    clients[i] = NULL;
  }

  lfd = net_listen(port, 0, 1);
  if(lfd < 0){
    return EX_UNAVAILABLE;
  }

  if(s->s_verbose){
    printf("%s: bof path:       %s\n", argv[0], s->s_bof_path ? : "<not set>");
    printf("%s: dev path:       %s\n", argv[0], s->s_dev_path ? : "<not set>");
    printf("%s: fpga number:    %d\n", argv[0], s->s_fpga_number);
    printf("%s: listening port: %s\n", argv[0], port);
  }

  /* WARNING: I have a sneaking suspicion that my shutdown logic isn't 100% */

  for(run = 1; run;){

    FD_ZERO(&fsr);
    FD_ZERO(&fsw);

    used = 0;
    max = (-1);
    for(i = 0; i < MAX_CLIENTS; i++){ /* could be made a list which grows instead of being fixed at MAX_CLIENTS */
      if(clients[i] == NULL){
        continue; /* empty slot */
      }
      used++;

      tfd = fileno_katcp(clients[i]);

      status = exited_katcp(clients[i]);
#ifdef DEBUG
      fprintf(stderr, "%s[%d]: status is %d, fd=%d\n", app, i, status, tfd);
#endif
      switch(status){
        case KATCP_EXIT_NOTYET : /* still running */
          FD_SET(tfd, &fsr);
          break;
        case KATCP_EXIT_QUIT : /* only this connection is shutting down */
          on_disconnect_katcp(clients[i], NULL);
          break;
        default : /* global shutdown */
          run = (-1); /* pre shutdown mode */
          on_disconnect_katcp(clients[i], NULL);
          for(j = 0; j < MAX_CLIENTS; j++){
            if(clients[j] && (!exited_katcp(clients[j]))){
              /* let the others know */
#ifdef DEBUG
              fprintf(stderr, "%s[%d]: terminating %d\n", app, i, j);
#endif
              terminate_katcp(clients[j], status);
            }
          }
          break;
      }
      if(flushing_katcp(clients[i])){ /* only write data if we have some */
#ifdef DEBUG
        fprintf(stderr, "%s[%d]: need to flush\n", app, i);
#endif
        FD_SET(tfd, &fsw);
      }

      /* WARNING: a bit risky: if running we aways read, if not we run on_disconnect which wants to do output, so we write, either way, the file descriptor should be part of the select */
      if(tfd > max){
        max = tfd;
      }
    }

    if(run > 0){ /* we are still running normally */
      if(used < MAX_CLIENTS){ /* do we have space fo new connections ? */
        FD_SET(lfd, &fsr);
        if(lfd > max){
          max = lfd;
        }
      }
    } else {
#ifdef DEBUG
      fprintf(stderr, "%s: heading toward shutdown: %d\n", app, run);
#endif
      if(used == 0){ /* no clients, change from pre shutdown to shutdown */
        run = 0;
      }
      tv.tv_sec = 1;
      tv.tv_usec = 0;
    }

    result = select(max + 1, &fsr, &fsw, NULL, run ? NULL : &tv);
    
    switch(result){
      case -1 :
        switch(errno){
          case EAGAIN :
          case EINTR  :
            continue; /* WARNING */
          default  :
            fprintf(stderr, "%s: select failed: %s\n", app, strerror(errno));
            return EX_OSERR;
        }
        break;
#if 0
      case  0 :
        /* continue is risky, later timer work may nail us here */
#endif
    }


    add = MAX_CLIENTS;

    for(i = 0; i < MAX_CLIENTS; i++){ /* there are better ways ... */
      if(clients[i] == NULL){
      	add = i;
      	continue;
      }
      tfd = fileno_katcp(clients[i]);

      if(FD_ISSET(tfd, &fsw)){
      	if(write_katcp(clients[i]) < 0){
#ifdef DEBUG
          fprintf(stderr, "%s[%d]: write failed\n", app, i);
#endif
          shutdown_katcp(clients[i]);
          clients[i] = NULL;
          add = i;
          continue;
        }
        /* if we are busy stopping and have flushed all io, then stop */
        if(exited_katcp(clients[i])){
          if(!flushing_katcp(clients[i])){
            shutdown_katcp(clients[i]);
            clients[i] = NULL;
            add = i;
            continue;
          }
        }
      }

      if(FD_ISSET(tfd, &fsr)){
#ifdef DEBUG
	fprintf(stderr, "%s: [%d] need to read\n", app, i);
#endif
        if(read_katcp(clients[i])){ /* end on eof or error, either way we release this entry */
          shutdown_katcp(clients[i]);
          clients[i] = NULL;
          add = i;
          continue;
        }
      }

      /* for commands taking lots of time, use lookup_katcp and call_katcp separately */
      if(dispatch_katcp(clients[i]) < 0){
        shutdown_katcp(clients[i]);
        clients[i] = NULL;
        add = i;
        continue;
      }

    }

    /* do we have a new connection and space for it ? */
    if(FD_ISSET(lfd, &fsr) && (add < MAX_CLIENTS)){
#ifdef DEBUG
      if((add >= MAX_CLIENTS) || (clients[add] != NULL)){
      	fprintf(stderr, "%s: major logic failure: expected empty slot (%d)\n", app, add);
      	abort();
      }
#endif
      len = sizeof(struct sockaddr_in);
      tfd = accept(lfd, (struct sockaddr *) &remote, &len);
      if(tfd < 0){
      	fprintf(stderr, "%s: unable to accept: %s\n", app, strerror(errno));
      } else {
        len = sizeof(struct sockaddr_in);
        ptr = inet_ntoa(remote.sin_addr);
        strncpy(buffer, ptr, NTOA_BUFFER);

        fprintf(stderr, "%s: [%d] new connection %s:%d %d/%d\n", app, add, ptr, ntohs(remote.sin_port), used + 1, MAX_CLIENTS);
        /* set up state for a connection */

      	clients[add] = setup_katcp(tfd);
#ifdef DEBUG
        fprintf(stderr, "%s[%d]: new connection: fd=%d\n", app, add, tfd);
#endif

      	if(clients[add] == NULL){

      	  fprintf(stderr, "%s: unable create new client instance\n", app);
        }

        do_startup(clients[add], s);

        /* an alternative to the above setup could be a clone of a template instance, but note that clone won't duplicate private state */

        on_connect_katcp(clients[add]);

        if(getsockname(tfd, (struct sockaddr *) &local, &len) >= 0){
          log_message_katcp(clients[add], KATCP_LEVEL_INFO, SUBSYSTEM, "new connection %s:%u to %s:%u", buffer, ntohs(remote.sin_port), inet_ntoa(local.sin_addr), ntohs(local.sin_port));
        }

      }
    }
  }

  return 0;
}

