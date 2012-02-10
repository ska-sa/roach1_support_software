
/* an attempt a a borph server which speaks the kat control protocol */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sysexits.h>
#include <dirent.h> 

#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <katcp.h>

#define DEFAULT_LISTCMD_PATH "/usr/bin" /* path to executable commands */
#define DEFAULT_BOF_PATH "/boffiles"    /* path to executable bof files */

#define LOADTIME 5                      /* wait XX seconds after loading a bitstream before trying to read the list of devices */

#define SUBSYSTEM "abs"                 /* name under which errors are reported */

struct abs_entry{
	int e_fd;
	unsigned int e_size;
	char *e_name;
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

        s->s_size++;
        return 0;
      }
    }
    close(fd);
  }

  return -1;
}

static void check_status(struct katcp_dispatch *d)
{
  struct abs_state *s;
  pid_t collect;
  int status;

  s = get_state_katcp(d);

  while((collect = waitpid(-1, &status, WNOHANG)) > 0){
  	if(collect == s->s_pid){
  		s->s_pid = (-1);
    }
    if(WIFEXITED(status) && (WEXITSTATUS(status) == 0)){
      log_message_katcp(d, KATCP_LEVEL_INFO, SUBSYSTEM, "process id %d exited normally");
    } else {
      log_message_katcp(d, KATCP_LEVEL_WARN, SUBSYSTEM, "process id %d exited abnormally", collect);
    }
  }
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

  check_status(d);

  if(s->s_pid > 0){
    if(kill(s->s_pid, SIGKILL)){
    	if(errno == ESRCH){ /* already gone */
        log_message_katcp(d, KATCP_LEVEL_WARN, SUBSYSTEM, "process id %d already gone", s->s_pid);
      } else {
        log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "unable to end process id %d", s->s_pid);
        return -1;
      }
    } else {
      log_message_katcp(d, KATCP_LEVEL_INFO, SUBSYSTEM, "killed process id %d", s->s_pid);
    }
    s->s_pid = (-1);
  }

  /* no arguments seems to indicate just to stop a process */
  if(argc <= 1){
  	return 0;
  }

  image = arg_string_katcp(d, 1);
  if(image == NULL){
    log_message_katcp(d, KATCP_LEVEL_FATAL, SUBSYSTEM, "unable to acquire device argument (arg count %d)", argc);
    return -1;
  }

  i = log_message_katcp(d, KATCP_LEVEL_DEBUG, SUBSYSTEM, "attempting to program %s", image);
#ifdef DEBUG
	fprintf(stderr, "log message returns %d\n", i);
#endif

  if(strchr(image, '/')){
    log_message_katcp(d, KATCP_LEVEL_FATAL, SUBSYSTEM, "client attempts to specify a path (%s)", image);
    return -1;
  }

  i = strlen(s->s_bof_path) + strlen(image) + 2;

  tmp = realloc(s->s_image, i);
  if(tmp == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "unable to allocate %d bytes", i);
  	return -1;
  }

  s->s_image = tmp;
  snprintf(s->s_image, i, "%s/%s", s->s_bof_path, image);

  if(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "unable to create socketpair: %s", strerror(errno));
    return -1;
  }

  s->s_pid = fork();
  if(s->s_pid < 0){
    log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "unable to spawn process: %s", strerror(errno));
    return -1;
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
    log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "unable to allocate memory");
  	return -1;
  }

  flush_katcp(d); /* do some io before we stall */
  sleep(LOADTIME); /* WARNING, stalls everything */

  dr = opendir(s->s_process);
  if(dr == NULL){
  	tmp = strerror(errno);
#ifdef DEBUG
  	fprintf(stderr, "progdev: code %d maps to <%s>\n", errno, tmp);
#endif
    log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "unable to open %s: %s", s->s_process, tmp);
    /* TODO: kill process ? empty out process name ? */
    return -1;
  }

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

  return 0;
}

int status_cmd(struct katcp_dispatch *d, int argc)
{
  struct abs_state *s;

  check_status(d);

  s = get_state_katcp(d);

  if((s->s_pid > 0) && (s->s_process)){

#ifdef DEBUG
    fprintf(stderr, "status: can report %s\n", s->s_process);
#endif

    send_katcp(d, KATCP_FLAG_FIRST | KATCP_FLAG_STRING, "!status", KATCP_FLAG_STRING, KATCP_OK, KATCP_FLAG_LAST | KATCP_FLAG_STRING, s->s_process);
    return 1;
  }

  log_message_katcp(d, KATCP_LEVEL_INFO, SUBSYSTEM, "no process running");
  return -1;
}

int listbof_cmd(struct katcp_dispatch *d, int argc)
{
  struct abs_state *s;

  s = get_state_katcp(d);
  if(s == NULL){
  	return -1;
  }

	return display_list(d, s->s_bof_path);
}

int listcmd_cmd(struct katcp_dispatch *d, int argc)
{
  struct abs_state *s;

  s = get_state_katcp(d);
  if(s == NULL){
  	return -1;
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
  	return -1;
  }

  label = arg_string_katcp(d, 0);
  if(label == NULL){
    log_message_katcp(d, KATCP_LEVEL_FATAL, SUBSYSTEM, "internal logic failure");
  	return -1;
  }

  for(i = 0; i < s->s_size; i++){
    send_katcp(d,
        KATCP_FLAG_FIRST | KATCP_FLAG_STRING | KATCP_FLAG_MORE, "#", 
        KATCP_FLAG_STRING, label + 1,
        KATCP_FLAG_LAST | KATCP_FLAG_STRING, s->s_table[i].e_name);
  }
  
  /* TODO: possibly return the count with the ok message */

  return 0;
}

static int do_read_cmd(struct katcp_dispatch *d, int argc, int symbolic, int format)
{
	char *name;
	unsigned int start, length;
	unsigned long value;
  struct abs_state *s;
  int index, rr, have, fd, i, flags;
  char *ptr, *cmd;

  s = get_state_katcp(d);
  if(s == NULL){
  	return -1;
  }

	if(argc <= 1){
    log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "need a register to read, followed by optional offset and count");
		return -1;
  }

  cmd = arg_string_katcp(d, 0);
  if(cmd == NULL){
    log_message_katcp(d, KATCP_LEVEL_FATAL, SUBSYSTEM, "internal logic failure");
  	return -1;
  }

  length = 1;
  start = 0;

  if(symbolic){
    name = arg_string_katcp(d, 1);
    if(name == NULL){
      log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "no name available");
      return -1;
    }
    for(index = 0; (index < s->s_size) && strcmp(name, s->s_table[index].e_name); index++);
    if(index >= s->s_size){
      log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "%s unknown", name);
      return -1;
    }
  } else {
  	index = arg_unsigned_long_katcp(d, 1);
    if(index >= s->s_size){
      log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "%u out of range", index);
      return -1;
    }
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
  	return -1;
  }

#if 0
  base = strlen(s->s_process) + 1;
#endif

  log_message_katcp(d, KATCP_LEVEL_DEBUG, SUBSYSTEM, "%s:%@%u+%u", cmd + 1, name, start, length);

  fd = s->s_table[index].e_fd;
  if(start + length > s->s_table[index].e_size){
    log_message_katcp(d, KATCP_LEVEL_WARN, SUBSYSTEM, "range-fail:%u+%u>%u", start, length, s->s_table[index].e_size);
  }

  if(lseek(fd, start, SEEK_SET) != start){
    log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "seek-fail:%u", start);
    return -1;
  }

  have = 0;

  ptr = malloc(length);
  if(ptr == NULL){
  	return -1;
  }

  do{
  	rr = read(fd, ptr + have, length - have);
  	if(rr > 0){
  		have += rr;
    }
    /* WARNING doesn't deal with EINTRs */
  } while((have < length) && (rr > 0));

  if(have != length){
    log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "read-fail:%u", have);
  	free(ptr);
  	return -1;
  }

  append_string_katcp(d, KATCP_FLAG_FIRST | KATCP_FLAG_MORE | KATCP_FLAG_STRING, "!");
  append_string_katcp(d, KATCP_FLAG_STRING, cmd + 1);
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
  return 1;
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
  char *ptr, *cmd;
  struct stat st;

  s = get_state_katcp(d);
  if(s == NULL){
  	return -1;
  }

	if(argc <= 3){
    log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "need a register, offset and data to write");
		return -1;
  }

  cmd = arg_string_katcp(d, 0);
  if(cmd == NULL){
    log_message_katcp(d, KATCP_LEVEL_FATAL, SUBSYSTEM, "internal logic failure");
  	return -1;
  }

  length = 1;
  start = 0;

  if(symbolic){
    name = arg_string_katcp(d, 1);
    if(name == NULL){
      log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "no name available");
      return -1;
    }
    for(index = 0; (index < s->s_size) && strcmp(name, s->s_table[index].e_name); index++);
    if(index >= s->s_size){
      log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "%s unknown", name);
      return -1;
    }
  } else {
  	index = arg_unsigned_long_katcp(d, 1);
    if(index >= s->s_size){
      log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "%u out of range", index);
      return -1;
    }
  }

  fd = s->s_table[index].e_fd;

  start = arg_unsigned_long_katcp(d, 2);
  if(format > 1){
  	start  *= format;
  }
  if(lseek(fd, start, SEEK_SET) != start){
    log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "seek-fail:%u", start);
    return -1;
  }

  total = 0;

  for(i = 3; i < argc; i++){
    if(arg_null_katcp(d, i)){
      log_message_katcp(d, KATCP_LEVEL_ERROR, SUBSYSTEM, "empty data field");
      return -1;
    }

    switch(format){
      case 1 :
        ucvalue = arg_unsigned_long_katcp(d, i);
        wr = write(fd, &ucvalue, 1);
        if(wr != 1){
          log_message_katcp(d, KATCP_LEVEL_WARN, SUBSYSTEM, "unable to write byte: %s", strerror(errno));
          return -1;
        }
        break;
      case sizeof(unsigned int) :
        uivalue = arg_unsigned_long_katcp(d, i);
        wr = write(fd, &uivalue, sizeof(unsigned int));
        if(wr != sizeof(unsigned int)){
          log_message_katcp(d, KATCP_LEVEL_WARN, SUBSYSTEM, "unable to write %d bytes: %s", sizeof(unsigned int), strerror(errno));
          return -1;
        }
        total += wr;
        break;
      default :

        length = arg_buffer_katcp(d, i, NULL, 0);
        /* WARNING: being too chummy with API internals */
        ptr = arg_string_katcp(d, i);

        if((length <= 0) || (ptr == NULL)){
          log_message_katcp(d, KATCP_LEVEL_FATAL, SUBSYSTEM, "internal problems, unable to access argument %d", i);
          return -1;
        }
        j = 0;
        while(j < length){
          wr = write(fd, ptr + j, length - j);
          if(wr <= 0){
            log_message_katcp(d, KATCP_LEVEL_WARN, SUBSYSTEM, "unable to write %d bytes at %d: %s", length - j, start + j, strerror(errno));
            return -1;
          } 
          j += wr;
          total += wr;
        }
        break;
    }
  }

  log_message_katcp(d, KATCP_LEVEL_DEBUG, SUBSYSTEM, "wrote %u bytes", total);

  if(fstat(fd, &st)){
    log_message_katcp(d, KATCP_LEVEL_WARN, SUBSYSTEM, "unable to do final stat: %s", strerror(errno));
  }

  if(s->s_table[index].e_size != st.st_size){
    log_message_katcp(d, KATCP_LEVEL_INFO, SUBSYSTEM, "file size updated to %u", (unsigned int) st.st_size); 
  }

  s->s_table[index].e_size = st.st_size;

  return 0;
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

  return 1;
}
#endif

int main(int argc, char **argv)
{
  struct katcp_dispatch *d;
  struct abs_state state, *s;
  char *port;
  int i, j, c, result;

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
          printf("Usage: %s [-b bof path] [-f FPGA number] [-p port] [-d register path]\n", argv[0]);
          printf("  -d will fake registers from a specified location\n");
          return 0;
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

  d = startup_version_katcp(SUBSYSTEM, 0, 1);
  if(d == NULL){
    fprintf(stderr, "%s: unable to initialise state\n", argv[0]);
    return 1;
  }

  printf("%s: bof path:       %s\n", argv[0], s->s_bof_path ? : "<not set>");
  printf("%s: dev path:       %s\n", argv[0], s->s_dev_path ? : "<not set>");
  printf("%s: fpga number:    %d\n", argv[0], s->s_fpga_number);
  printf("%s: listening port: %s\n", argv[0], port);

  set_state_katcp(d, s);

  result = 0;

  result += register_katcp(d, "?progdev",  "programs an image", &progdev_cmd);
  result += register_katcp(d, "?status",   "displays image status information", &status_cmd);
  result += register_katcp(d, "?listbof",  "displays available images", &listbof_cmd);
  result += register_katcp(d, "?listcmd",  "displays available shell commands", &listcmd_cmd);
  result += register_katcp(d, "?listdev",  "displays available device registers", &listdev_cmd);

  result += register_katcp(d, "?read",     "reads arbitrary data lengths from a named register", &read_cmd);
  result += register_katcp(d, "?indexread","reads arbitrary data lengths from a numbered register", &indexread_cmd);
  result += register_katcp(d, "?wordread", "reads data words from named a register", &wordread_cmd);

  result += register_katcp(d, "?write",     "writes arbitrary data lengths to a named register", &write_cmd);
  result += register_katcp(d, "?indexwrite","writes arbitrary data lengths to a numbered register", &indexwrite_cmd);
  result += register_katcp(d, "?wordwrite", "writes data words to a named register", &wordwrite_cmd);

  if(result){
    fprintf(stderr, "%s: unable to enroll commands\n", argv[0]);
    return 1;
  }

  if(run_server_katcp(d, port, 0) < 0){
    fprintf(stderr, "%s: run failed\n", argv[0]);
    return 1;
  }

  shutdown_katcp(d);

  return 0;
}

#if 0

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <dirent.h> 
#include <sys/param.h> 
#include <sys/stat.h> 
#include <signal.h>
#include <sys/wait.h>
#include "debug_macros.h"

#define MAXBUFLEN 4096
#define MAXARGS 64      //the maximum number of arguments that can be passed to exec
#define MAXCMDLEN 64    //the maximum size of a command
#define LOADTIME 5     //wait XX seconds after loading a bitstream before trying to read the list of devices
#define SIGKILL 9

//PACKET TYPES:
//=============
#define PROGDEV  10
#define STATUS   20
#define LISTCMD  30
#define LISTBOF  35
#define LISTDEV  40
#define READ     50
#define INDEXEDREAD 51
#define WRITE    60
#define INDEXEDWRITE 61
#define EXEC    70

#define ERR     110
#define REPORT  120
#define AVAILCMND 130
#define AVAILBOF  135
#define AVAILDEV  140
#define DATA    160
#define EXECREPORT 170

#define DEFAULT_LISTCMD_PATH "/usr/bin"       // path of executable commands
#define DEFAULT_BOF_PATH "/boffiles"        // path for executable bof files

//ERRORS:
//=======
#define ERRDIR "ERR Unable to open directory"
#define ERRSOCKFD "ERR Unable to set socket descriptor"
#define ERRSOCKBND "ERR Unable to bind to socket"
#define ERRRX "ERR Unable to receive packet"
#define ERRTX "ERR Unable to send packet"
#define ERRDEC "ERR decoding packet type"
#define ERRNOTCONFIG "ERR device not configured"
#define ERRCONFIG "ERR unable to configure device"
#define ERRFORK "ERR unable to start chid process"
#define ERRKILL "ERR unable to kill child process"
#define ERRNOPROC "ERR child process does not exist"
#define ERRFOPEN "ERR could not open file"
#define ERRREAD "ERR read packet format error"
#define ERRINDEXEDREAD "ERR indexed read packet format error"
#define ERRWRITE "ERR write packet format error"
#define ERRINDEXEDWRITE "ERR indexed write packet format error"
#define ERRBOUNDRY "ERR out of bound read/write request"
#define ERRDEVNOTFOUND "ERR device not found"
#define ERRINDOUTOFBOUNDS "ERR device index out of bounds"
#define ERRFLUSH "ERR flushing data to file"
#define ERREXEC "ERR could not execute command"

extern char *optarg;

int sockfd;
struct sockaddr_in my_addr;    // server's address information
struct sockaddr_in their_addr; // connector's address information
void reply(int num_bytes);
void reply_error(char * errormessage);
int sendall(int sfd, char *buf, int len, const struct sockaddr *to, socklen_t tolen);
int get_int(unsigned char msb, unsigned char msb2, unsigned char lsb2, unsigned char lsb);
char buf[MAXBUFLEN];
void read_from_device(FILE* stream, unsigned int offset, unsigned int trans_size);
void write_to_device(FILE* stream, unsigned int offset, unsigned int trans_size, int bufOffset);


int main(int argc, char ** argv)
{
    socklen_t addr_len;  
    int fd[2]; //used solely to redirect stdin for the purposes of backgrounding
    int num_bytes;
    unsigned int counter;  
    unsigned int counter2;
    DIR           *d;
    struct dirent *dir;
    int pid = -1;
    int exec_pid = -1;
    char *dev_path = NULL;
    char proc[200];
    FILE *stream;
    FILE **p_devfp;  // open all registers
    int *p_devsize;
    char **p_devname;
    unsigned int offset;
    unsigned int trans_size;
    char reg_name[64];
    char reg_qual[128];
    char cmd[MAXCMDLEN]; // how long can this be????
    char dev[64];
    int len_name;
    int num_dev;
    char *exec_argv[MAXARGS];
    int c;
    char *bof_path = DEFAULT_BOF_PATH;
    // only put this in if we want to force the path the EXEC commands are executed from
    //char *exec_path = DEFAULT_EXEC_PATH;
    int fpga_number;
    int port=4950;
    //read in command line args
    while((c = getopt(argc, argv, "b:p:e:f:d:h")) != -1){
        switch(c) {
            // command line path to find bof files
            case 'b':   
                bof_path=optarg;
                debug_fprintf(stderr,"Command line arg bof path: %s\n", bof_path);
                break;
            case 'd':
                dev_path=optarg;
                debug_fprintf(stderr,"Development mode. Will fake registers from: %s\n", dev_path);
            // only put this in if we want to force the path the EXEC commands are executed from
            //case 'e':   
            //    exec_path=optarg;
            //    debug_fprintf(stderr,"Command line arg exec path: %s\n", exec_path);
            //    break;
            case 'f':   
                fpga_number=atoi(optarg);
                debug_fprintf(stderr,"Command line arg FPGA number: %d\n", fpga_number);
                break;
            case 'p':
                port = atoi(optarg);
                break;
            //display help
            case 'h':  
                printf("Usage: %s [-b bof path] [-f FPGA number] [-p port] [-d register path]\n -d will fake registers from a specified location.\n", argv[0]);
                return 0;
        } // end switch option
    } //end while(get options)
    
    
    // obtain a socket descriptor
    if ((sockfd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {        
        perror(ERRSOCKFD);
        exit(1);
    }
    
    my_addr.sin_family = AF_INET;         // host byte order
    my_addr.sin_port = htons(port);     // short, network byte order
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY); // automatically fill with my IP  ... doesn't matter if we use hton on INASSR_ANY or not
    // since it's value is zero
    memset(my_addr.sin_zero, '\0', sizeof my_addr.sin_zero);


/*    // lose the pesky "Address already in use" error message
    if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,1,sizeof(int)) == -1) {
	    perror("setsockopt");
	    exit(1);
    } 
*/    
    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof my_addr) == -1) {
        perror(ERRSOCKBND);
        exit(1);
    }
    
    printf("Listening on IP address %s on port %i\n",inet_ntoa(my_addr.sin_addr),port);
    
    addr_len = sizeof their_addr;
    
    while (1) {
        // read a packet in, exit if there is an error reading
        if ((num_bytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0, (struct sockaddr *)&their_addr, &addr_len)) == -1) {
            perror(ERRRX);
            exit(1);
        }
        
        debug_fprintf(stderr,"Packet from  %s with len %d \n",inet_ntoa(their_addr.sin_addr),num_bytes);
        buf[num_bytes] = '\0';
        debug_fprintf(stderr,"CONTENTS: %s\n", buf);
        
        // process the command 
        switch (buf[0]) {
                //program the FPGA and keep track of the child's process id.
            case    PROGDEV :   
                debug_fprintf(stderr,"PROGDEV:");
                
                //unload the previous design if necessary
                if (pid>0){ 
                    debug_fprintf(stderr,"Closing %i open files...\n",num_dev);
                    for(counter=0; counter<num_dev; counter++)
                    {
                        fclose(p_devfp[counter]);
                    }
                    num_dev=0;
                    
                    debug_fprintf(stderr,"Trying to kill process %i...\n",pid);
                    counter = kill(pid,SIGKILL);
                    if (counter != 0){
                        reply_error(ERRKILL);
                        debug_fprintf(stderr,ERRKILL);
                        if (counter == ESRCH){
                            reply_error(ERRNOPROC);
                            debug_fprintf(stderr,ERRNOPROC);
                        }
                    }
                    wait(NULL);
                    pid = -1;      
                    proc[0] = 0;
                }
                
                //check to see if the user is trying to load a new bof file
                if (num_bytes>2){ 
                    
                    snprintf(cmd, MAXCMDLEN,"%s/%s",bof_path, &buf[1]);
                    
                    debug_fprintf(stderr,"Cmd is: %s with len: %d\n", cmd, strlen(cmd));
                    //Sneakily redirect standard input to prevent stopping when backgrounding 
                    debug_fprintf(stderr,"making a fd...\n");
                    pipe(fd);
                    dup2(fd[0],0);
                    debug_fprintf(stderr,"Forking...\n");
                    pid=fork();                                
                    
                    if (pid < 0){ //failed.
                        debug_fprintf(stderr,ERRFORK);
                        reply_error(ERRFORK);
                    } else if (pid == 0) { //child process
                        if (execlp(cmd , cmd, (char *) 0) < 1) {
                            dup2(fd[0],0);
                            debug_fprintf(stderr,ERRCONFIG);
                            reply_error(ERRCONFIG);
                        }
                        // exit if child could'nt execute the cmd
                        exit(1);
                    } else {    //success, parent process!
                        if (dev_path){
                            sprintf(proc,"%s",dev_path);
                        }
                        else{
                            sprintf(proc,"/proc/%i/hw/ioreg",pid);
                        }
                        debug_fprintf(stderr,"New bitstream: %s with registers in %s\n",cmd,proc);     
                        
                        sleep(LOADTIME); //wait 'till the FPGA is programmed before trying to read registers 

                        // check that we can open the directory ok
                        if ((d = opendir(proc))==NULL){ 
                            debug_fprintf(stderr,ERRDIR);  
                            kill(pid,SIGKILL);
                            pid = -1;
                            proc[0]=0;
                            break;
                        }
                        rewinddir(d);
                            
                        // generate dev_table here, every time FPGA is programed
                        counter =0;
                        while ( (dir = readdir(d)) ) 
                        {
                            if ( strcmp( dir->d_name, ".") == 0 || strcmp( dir->d_name, "..") ==0){
                                debug_fprintf(stderr,"Ignoring . or ..\n");
                                continue;
                            }
                            else {
                                debug_fprintf(stderr,"found file: %d: %s\n",counter, dir->d_name);
                                ++counter;
                            }
                        }
                        rewinddir(d);
                        num_dev = counter; 
                        
                        debug_fprintf(stderr,"number of devs in proc directory: %d\n", counter);
                        
                        p_devfp = (FILE **)malloc(num_dev*(sizeof(FILE *)));
                        p_devsize = (int *)malloc(num_dev*(sizeof(int)));
                        p_devname = (char **)malloc(num_dev*(sizeof(char *)));
                        
                        
                        counter = 0;
                        while ( (dir = readdir(d)) ) {
                            // copy the file name so long as it isn't '.' or '..'
                            if ( !(strcmp( dir->d_name, ".")==0 || strcmp( dir->d_name, "..") ==0) )
                            {
                                // dev name indexes
                                p_devname[counter] = (char *)malloc(strlen(dir->d_name)+1);
                                strcpy( p_devname[counter], dir->d_name);               
                                // dev file indexes
                                sprintf( dev, "%s/%s", proc, p_devname[counter]);
                                debug_fprintf(stderr,"Opening %i: %s...", counter,dev);
                                if ((p_devfp[counter] = fopen(dev,"r+")) != NULL) {
                                    debug_fprintf(stderr, "Success!");
                                    //seek to the end to get the size of the file
                                    fseek(p_devfp[counter], 0L, SEEK_END);
                                    p_devsize[counter] = ftell(p_devfp[counter]);
                                    debug_fprintf(stderr," Size: %d\n", p_devsize[counter]);  
                                }
                                else{
                                    debug_fprintf(stderr,"ERR opening p_devfp[%i]:%s\n",counter,p_devname[counter]);
                                }
                                ++counter;                                      
                            } 
                        } // end while we're still getting a list of files and their sizes
                    }
                } //end if we're trying to program the FPGA
                break;
                
                case    STATUS :    //return a string representing the filesystem location of the process, or error if FPGA is not yet configured.
                debug_fprintf(stderr,"STATUS:\n");
                //check if FPGA is configured
                if (pid < 0){
                    reply_error(ERRNOTCONFIG);
                    debug_fprintf(stderr,ERRNOTCONFIG);
                } else{
                    buf[0] = REPORT;
                    for (counter=0;counter<strlen(proc);counter++){ 
                        buf[counter+2] = proc[counter];
                    }
                    reply(counter+2);
                    debug_fprintf(stderr,"%s\n",proc);
                }
                break;
                
                case    LISTCMD :   //Iterate through each item in the CMD_PATH directory and add it to the buf buffer, along with its length
                debug_fprintf(stderr,"LISTCMD\n"); 
                buf[0] = AVAILCMND; //give the reply packet a type. Tag id remains as it was in buf[1]
                counter = 2;
                if ((d = opendir(DEFAULT_LISTCMD_PATH)) == NULL){
                    //perror(ERRDIR);
                    debug_fprintf(stderr,ERRDIR);
                    reply_error(ERRDIR);
                    
                } else {
                    while( ( dir = readdir( d ) ) ) {
                        if( strcmp( dir->d_name,"." ) == 0 || strcmp( dir->d_name, ".." ) == 0 ) {
                            continue;
                        } else strcpy( &buf[counter], dir->d_name);
                        
                        counter += (strlen(dir->d_name)+1);
                        debug_fprintf(stderr,"LISTCMD: dir->d_name *: %s\n", dir->d_name);
                        
                    } //end while still stuff in the directory
                } //end else if the directory does not exist
                closedir( d );  
                
                debug_fprintf(stderr,"LISTCMD: sending %d bytes.\n", counter); 
                reply(counter);
                break;
                
                case    LISTBOF :   //Iterate through each item in the BOF_PATH directory and add it to the buf buffer, along with its length
                debug_fprintf(stderr,"LISTBOF\n"); 
                buf[0] = AVAILBOF; //give the reply packet a type. Tag id remains as it was in buf[1]
                counter = 2;
                if ((d = opendir(DEFAULT_BOF_PATH)) == NULL){
                    debug_fprintf(stderr,ERRDIR);
                    reply_error(ERRDIR);
                    
                } else {
                    while( ( dir = readdir( d ) ) ) {
                        if( strcmp( dir->d_name,"." ) == 0 || strcmp( dir->d_name, ".." ) == 0 ) {
                            continue;
                        } else strcpy( &buf[counter], dir->d_name);
                        
                        counter += (strlen(dir->d_name)+1);
                        debug_fprintf(stderr,"LISTBOF: dir->d_name *: %s\n", dir->d_name);
                        
                    } //end while still stuff in the directory
                } //end else if the directory does not exist
                closedir( d );  
                
                debug_fprintf(stderr,"LISTBOF: sending %d bytes.\n", counter); 
                reply(counter);
                break;
                
                case    LISTDEV :   
                debug_fprintf(stderr,"LISTDEV\n");
                if (pid > 0){ 
                    buf[0] = AVAILDEV; //give the reply packet a type. Tag id remains as it was in buf[1]
                    debug_fprintf(stderr,"PID IS %i. with %i devices.\n",pid, num_dev);
                    counter = 2;      
                    for (counter2 = 0; counter2 < num_dev; counter2++) {
                        debug_fprintf(stderr,"LISTDEV: buffering %s\n",p_devname[counter2]);
                        strcpy(&buf[counter], p_devname[counter2]);
                        counter += (strlen(p_devname[counter2])+1);
                    }
                    
                    debug_fprintf(stderr,"LISTDEV: num bytes transmited: %d\n", counter);
                    reply(counter); 
                } else {
                    reply_error(ERRNOTCONFIG);
                }
                break;
                
                case    READ :      //read a user-specified file
                debug_fprintf(stderr,"READ:\n");
                if (pid>0) {
                    //parse the request:
                    if (num_bytes > 10){
                        offset = get_int(buf[2],buf[3],buf[4],buf[5]);
                        trans_size = get_int(buf[6],buf[7],buf[8],buf[9]);
                        
                        for (counter=10;counter<num_bytes; counter++){
                            reg_name[counter-10] = buf[counter];
                            //printf("counter: %i, char: %c\n",counter,reg_name[counter-10]);
                        }
                        reg_name[counter-10] = 0;    
                        
                        sprintf(reg_qual,"%s/%s",proc,reg_name);
                        debug_fprintf(stderr,"READ: looking up file %s from index %i for %i bytes.\n",reg_qual,offset,trans_size);
                        
                        stream = fopen(reg_qual,"r");
                        if (stream !=NULL){	  
                            debug_fprintf(stderr,"Success openning the file!\n");
                            read_from_device(stream, offset, trans_size);
                            fclose(stream);
                        } 
                        else {
                            reply_error(ERRFOPEN);
                            debug_perror(ERRFOPEN);
                        }
                    }  // if we got at least 12 bytes, register_name at least of length 2 (null terminated)
                    else {   
                        reply_error(ERRREAD);
                        //perror(ERRREAD);
                    }    
                } else { //the device isn't programmed yet (pid<=0)
                    reply_error(ERRNOTCONFIG);
                }
                break;            
                
                //read from the device using an index
                case    INDEXEDREAD:
                debug_fprintf(stderr,"INDEXEDREAD:\n");
                if (pid>0) {
                    //parse the request:
                    if (num_bytes == 11){
                        // need to change get_int definition for netcat testing ******************
                        offset = get_int(buf[2],buf[3],buf[4],buf[5]);
                        trans_size = get_int(buf[6],buf[7],buf[8],buf[9]);
                        for (counter=0;counter<num_bytes;counter++){
                            debug_fprintf(stderr,"counter: %i, ASCI: %i char: %c\n",counter,buf[counter],buf[counter]);
                        }    
                        
                        int dev_index = (int) buf[10];
                        
                        //if the register is found, write to it
                        if(dev_index < num_dev)
                        {
                            read_from_device(p_devfp[dev_index], offset, trans_size);
                        }
                        else{
                            reply_error(ERRINDOUTOFBOUNDS);
                            debug_fprintf(stderr,ERRINDOUTOFBOUNDS);
                        }
                        
                    }  // if we got at least 11 bytes, register_name at least of length 2 (null terminated)
                    else {   
                        reply_error(ERRINDEXEDREAD);
                        debug_fprintf(stderr,ERRINDEXEDREAD);
                    }    
                } 
                //the device isn't programmed yet (pid<=0)
                else { 
                    reply_error(ERRNOTCONFIG);
                }
                
                case    WRITE :     //write to a register
                debug_fprintf(stderr,"WRITE:");  
                if (pid>0) {
                    //parse the request:
                    if (num_bytes > 11){
                        offset = get_int(buf[2],buf[3],buf[4],buf[5]);
                        trans_size = get_int(buf[6],buf[7],buf[8],buf[9]);
                        
                        debug_fprintf(stderr,"offset: %u\n", offset);
                        debug_fprintf(stderr,"trans_size: %u\n", trans_size);
                        
                        counter = 0;
                        
                        while (counter <63) {
                            if ( (reg_name[counter]=buf[counter+10]) == 0 ) break;
                            ++counter;
                        }
                        
                        // for safety
                        reg_name[counter]='\0';
                        
                        sprintf(reg_qual,"%s/%s",proc,reg_name);
                        debug_fprintf(stderr,"looking up file %s from index %i for %i bytes\n",reg_qual,offset,trans_size);
                        
                        
                        // get the length of the register name including the null termination
                        len_name = strlen(reg_name)+1;
                        debug_fprintf(stderr,"reg_name: %s\n", reg_name);
                        debug_fprintf(stderr,"len_name: %d\n", len_name);
                        
                        //search for reg name in table
                        for(counter=0; (counter<num_dev) && (strcmp(p_devname[counter],reg_name) != 0) ; counter++);
                        
                        //if the register is found, write to it
                        if(counter < num_dev)
                        {
                            debug_fprintf(stderr, "file pointer found!\n");
                            write_to_device(p_devfp[counter], offset, trans_size, 10+len_name);                     
                            
                        } else{
                            reply_error(ERRDEVNOTFOUND);
                            debug_fprintf(stderr,ERRDEVNOTFOUND);
                        }
                    } //if we got at least 11 bytes since  len(reg_name), reg_name and Data need to be at least 1 byte. 
                    else {
                        reply_error(ERRWRITE);
                        debug_perror(ERRWRITE);
                    }
                } 
                //the device isn't programmed yet (pid<=0)
                else { 
                    reply_error(ERRNOTCONFIG);
                }
                break;              
                
                case    INDEXEDWRITE:
                debug_fprintf(stderr,"INDEXEDWRITE:");  
                if (pid>0) {
                    //parse the request:
                    if (num_bytes > 11){
                        offset = get_int(buf[2],buf[3],buf[4],buf[5]);
                        trans_size = get_int(buf[6],buf[7],buf[8],buf[9]);
                        
                        debug_fprintf(stderr,"offset: %u\n", offset);
                        debug_fprintf(stderr,"trans_size: %u\n", trans_size);
                        
                        int dev_index = (int) buf[10];
                        
                        //if the register is found, write to it
                        if(dev_index < num_dev)
                        {
                            debug_fprintf(stderr, "file pointer found!\n");
                            write_to_device(p_devfp[dev_index], offset, trans_size, 11);                     
                            
                        } 
                        else{
                            reply_error(ERRINDOUTOFBOUNDS);
                            debug_fprintf(stderr,ERRINDOUTOFBOUNDS);
                        }
                    } //if we got at least 11 bytes since Data needs to be at least 1 byte. 
                    else {
                        reply_error(ERRINDEXEDWRITE);
                        debug_perror(ERRINDEXEDWRITE);
                    }      
                    break;                          
                    
                    default :        
                    debug_fprintf(stderr,ERRDEC);
                    reply_error(ERRDEC);
                } 
                //the device isn't programmed yet (pid<=0)
                else { 
                    reply_error(ERRNOTCONFIG);
                }
                break;
                
                case        EXEC:

                // get the command and arguments: 
                exec_argv[0] = strtok(&(buf[2])," ");
                for (counter=1; counter<MAXARGS-1 && exec_argv[counter]!=NULL; counter++){
                    exec_argv[counter] = strtok(NULL," ");
                }
                exec_argv[counter] = NULL;
                
                debug_fprintf(stderr,"cmd is: %s\n", exec_argv[0]);
                debug_fprintf(stderr,"first argument is: %s\n",exec_argv[1]);
                debug_fprintf(stderr,"length: %d\n", (int) strlen(cmd));
                debug_fprintf(stderr,"forking!\n");

                exec_pid=fork();  
                if (exec_pid < 0){ //failed
                    debug_fprintf(stderr,ERRFORK);
                    reply_error(ERRFORK);
                } 
                else if (exec_pid == 0) { //child process
                    if (execvp(exec_argv[0] , exec_argv) < 1) {
                        debug_fprintf(stderr,ERREXEC);
                        reply_error(ERREXEC);
                    }
                    // exit if child couldn't execute the cmd
                    debug_fprintf(stderr,"EXEC didn't work... killing child process\n");
                    exit(1);
                }
                else { //parent reply with a success packet
                    buf[0] = EXECREPORT;
                    buf[2] = exec_pid;
                    reply(4);
                    debug_fprintf(stderr,"EXEC: started pid %i\n",exec_pid);
                }
                
                break;
        } //end switch                                 
    } //end while(1)
    
    
    close(sockfd);
    
    return 0;
}

/*
 // for testing purposes and to be able to use decimal numbers
 int get_int(unsigned char msb, unsigned char msb2, unsigned char lsb2, unsigned char lsb) {
 return ((msb-48)*1000)+((msb2-48)*100)+((lsb2-48)*10)+lsb-48;
 }
 */

int get_int(unsigned char msb, unsigned char msb2, unsigned char lsb2, unsigned  char lsb){
    return ((msb<<24)+(msb2<<16)+(lsb2<<8)+lsb);
}


void reply(int num_bytes){
    //int_bytes indicates how many bytes should be transmitted
    if ((sendall(sockfd, buf, num_bytes, (struct sockaddr *)&their_addr, sizeof their_addr)) == -1) {
        //perror(ERRTX);
        exit(1);
    }  
}

void reply_error(char * errormessage){
    int count;
    //int_bytes indicates how many bytes should be transmitted
    buf[0] = ERR;
    for (count=0;count<strlen(errormessage);count++)
        buf[2+count] = errormessage[count];
    if ((sendall(sockfd, buf, count+2,(struct sockaddr *)&their_addr, sizeof their_addr)) == -1) {
        //perror(ERRTX);
        exit(1);
    }  
}

int sendall(int sfd, char *buf, int len, const struct sockaddr *to, socklen_t tolen)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft = len; // how many we have left to send
    int n;
    while(total < len) {
        n = sendto(sfd, buf+total, bytesleft, 0, to, tolen);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }
    //len = total; // return number actually sent here
    return n==-1?-1:0; // return -1 on failure, 0 on success
}

void read_from_device(FILE* stream, unsigned int offset, unsigned int trans_size)
{
    unsigned int endPos;
    unsigned int counter;
    fseek(stream,0L, SEEK_END);
    endPos = ftell(stream);
    debug_fprintf(stderr,"endPos: %u\n",endPos);
    
    if (endPos >= (offset+trans_size)) {
        fseek(stream, offset, SEEK_SET);
        
        buf[0] = DATA;  //give the reply packet type. Tag id remains as it was in buf[1] 
        
        for(counter=0; counter<trans_size; counter++)
            buf[counter+2] = fgetc(stream);
        for(counter=0; counter < 2+trans_size ; counter++)
            debug_fprintf(stderr,"buf[%i]: %c\n",counter,buf[counter]);
        
        // 1+1+trans_size
        reply(2+trans_size);
        
    }	    
    else {
        reply_error(ERRBOUNDRY);
        debug_perror(ERRBOUNDRY);
    }
}

void write_to_device(FILE* stream, unsigned int offset, unsigned int trans_size, int bufOffset)
{
    unsigned int endPos;
    unsigned int counter;
    
    // need to check if the size of the file is large enough
    fseek(stream, 0L, SEEK_END);
    endPos = ftell(stream);
    debug_fprintf(stderr,"endPos: %u\n",endPos);
    
    if (endPos >= (offset+trans_size)) {
        fseek(stream, offset, SEEK_SET);
        
        for (counter = 0; counter<trans_size; counter++) {
            fputc(buf[bufOffset+counter], stream);
            debug_fprintf(stderr,"***: %c\n", buf[bufOffset+counter]);
        }
        //flush any buffered data
        if(fflush(stream) != 0)
        {
            perror(ERRFLUSH);
            reply_error(ERRFLUSH);
        }
    }
    else {
        reply_error(ERRBOUNDRY);
        debug_perror(ERRBOUNDRY);
    }
    
}
#endif
