#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>

#include <unistd.h>
#include <fcntl.h>

#include <sys/stat.h>

#include <netc.h>

#include "core.h"
#include "modes.h"

char *name_pce(struct poco_core_entry *pce)
{
  if(pce == NULL){
    return NULL;
  }

  return pce->e_name;
}

char *full_name_pce(struct poco_core_entry *pce)
{
  if(pce == NULL){
    return NULL;
  }

  return pce->e_full_name;
}

int write_name_pce(struct katcp_dispatch *d, char *name, void *buffer, unsigned int start, unsigned int length)
{
  struct poco_core_entry *pce;

  pce = by_name_pce(d, name);
  if(pce == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to locate register %s for writing", name);
    return -1;
  }

  return write_pce(d, pce, buffer, start, length);
}

int write_pce(struct katcp_dispatch *d, struct poco_core_entry *pce, void *buffer, unsigned int start, unsigned int length)
{
  int wr;
  unsigned int have;

  if(pce->e_seek){
    if(lseek(pce->e_fd, start, SEEK_SET) != start){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "seek to %u in %s failed: %s", start, pce->e_name, strerror(errno));
      return -1;
    }
    /* TODO: possible check for writes beyond file end ? */
  }

  if(length == 4){
    uint32_t value;
    value = *((uint32_t *)buffer);
    log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "writing 0x%08x to %s", value, pce->e_name);
  } else {
    log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "writing %d bytes to %s", length, pce->e_name);
  }

  for(have = 0; have < length; ){
    wr = write(pce->e_fd, buffer + have, length - have);
    if(wr > 0){
      have += wr;
    } else if(wr == 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "write on %s returns zero", pce->e_name);
      return -1;
    } else { /* wr < 0 */
      switch(errno){
        case EINTR :
        case EAGAIN :
          break;
        default :
          log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "write to %s at position %u failed: %s", pce->e_name, start + have, strerror(errno));
          return -1;
      }
    }
  }

  return have;
}

int read_name_pce(struct katcp_dispatch *d, char *name, void *buffer, unsigned int start, unsigned int length)
{
  struct poco_core_entry *pce;

  pce = by_name_pce(d, name);
  if(pce == NULL){
    log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "unable to locate register %s for reading", name);
    return -1;
  }

  return read_pce(d, pce, buffer, start, length);
}

int read_pce(struct katcp_dispatch *d, struct poco_core_entry *pce, void *buffer, unsigned int start, unsigned int length)
{
  int rr;
  unsigned int have;

  if(pce->e_seek){ /* seekable */

    if((start + length) > pce->e_size){
      log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "range check fails in %s: %u+%u>%u", pce->e_name, start, length, pce->e_size);
      return -1;
    }

    if(lseek(pce->e_fd, start, SEEK_SET) != start){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "seek to %u in %s failed: %s", start, pce->e_name, strerror(errno));
      return -1;
    }
  } else { /* not seekable */
    if(start != 0){
      log_message_katcp(d, KATCP_LEVEL_WARN, NULL, "can not seek on stream %s but trying it anyway", pce->e_name);
      lseek(pce->e_fd, start, SEEK_SET); /* seek anyway - borph fifo semantics are borked */
    }
  }

#if 0
  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "reading %u from %s at %u", length, pce->e_name, start);
#endif

  for(have = 0; have < length; ){
    rr = read(pce->e_fd, buffer + have, length - have);
    if(rr > 0){
      have += rr;
    } else if(rr == 0){
      log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "premature end of file %s at %u of %u", pce->e_name, have, pce->e_size);
      return -1;
    } else { /* rr < 0 */
      switch(errno){
        case EAGAIN :
        case EINTR  :
          break;
        default :
          log_message_katcp(d, KATCP_LEVEL_ERROR, NULL, "read on %s failed: %s", pce->e_name, strerror(errno));
          return -1;
      }
    }
  } 

  if(have == 4){
    uint32_t value;
    value = *((uint32_t *)buffer);
    log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "read 0x%08x from %s", value, pce->e_name);
  } else {
    log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "read %d bytes from %s", have, pce->e_name);
  }

  return have;
}

struct poco_core_entry *by_offset_pce(struct katcp_dispatch *d, unsigned int offset) 
{
  struct poco_core_state *cs;

  cs = get_mode_katcp(d, POCO_CORE_MODE);

  if(cs == NULL){
    return NULL;
  }

  if(offset >= cs->c_size){
    return NULL;
  }

  return &(cs->c_table[offset]);
}

struct poco_core_entry *by_name_pce(struct katcp_dispatch *d, char *name)
{
  struct poco_core_state *cs;
  int i;

  cs = get_mode_katcp(d, POCO_CORE_MODE);

  if(name == NULL){
#ifdef DEBUG
    fprintf(stderr, "pce: name is null\n");
#endif
    return NULL;
  }

  if(cs == NULL){
#ifdef DEBUG
    fprintf(stderr, "pce: state is invalid\n");
#endif
    return NULL;
  }

  for(i = 0; i < cs->c_size; i++){
    if(cs->c_table[i].e_name && !(strcmp(cs->c_table[i].e_name, name))){
      return &(cs->c_table[i]);
    }
  }

#ifdef DEBUG
  fprintf(stderr, "pce: no match for name %s\n", name);
#endif

  return NULL;
}

int clear_all_pce(struct katcp_dispatch *d)
{
  struct poco_core_state *cs;
	struct poco_core_entry *pce;
  int i;

  cs = get_mode_katcp(d, POCO_CORE_MODE);
  if(cs == NULL){
    return -1;
  }

  for(i = 0; i < cs->c_size; i++){
    pce = &(cs->c_table[i]);

    if(pce->e_fd >= 0){
      close(pce->e_fd);
      pce->e_fd = (-1);
    }

    if(pce->e_full_name){
      /* assume that name is a pointer into full_name */
#ifdef DEBUG
      if(pce->e_full_name > pce->e_name){
        fprintf(stderr, "pce: major logic problem: name %p not a pointer into full name %p\n", pce->e_name, pce->e_full_name);
        abort();
      }
#endif
      free(pce->e_full_name);
      pce->e_full_name = NULL;
      pce->e_name = NULL;
#ifdef DEBUG
    } else { 
      if(pce->e_name){
        fprintf(stderr, "pce: logic failure - full name null, but name set\n");
        abort();
      }
#endif
    }
  }

  cs->c_size = 0;

  return 0;
}

int insert_pce(struct katcp_dispatch *d, char *name)
{
  struct stat st;
  int fd, len;
  struct poco_core_entry *tmp;
  char *ptr;
  struct poco_core_state *cs;
  int base;

  cs = get_mode_katcp(d, POCO_CORE_MODE);
  if(cs == NULL){
    return -1;
  }

  tmp = realloc(cs->c_table, sizeof(struct poco_core_entry) * (cs->c_size + 1));
  if(tmp == NULL){
    return -1;
  }

  cs->c_table = tmp;
  tmp = &(cs->c_table[cs->c_size]);

  base = strlen(cs->c_borph_proc);
  len = base + strlen(name) + 2;

  ptr = malloc(len);
  if(ptr == NULL){
    return -1;
  }

  snprintf(ptr, len, "%s/%s", cs->c_borph_proc, name);
  fd = open(ptr, O_RDWR);
  log_message_katcp(d, KATCP_LEVEL_TRACE, NULL, "opened %s as %d", ptr, fd);

  if(fd >= 0){

    fcntl(fd, F_SETFD, FD_CLOEXEC);

    if(fstat(fd, &st) == 0){
      tmp->e_fd = fd;
      tmp->e_size = st.st_size;

      tmp->e_full_name = ptr;
      tmp->e_name = ptr + base + 1;

      tmp->e_seek = S_ISFIFO(st.st_mode) ? 0 : 1;

      cs->c_size++;
      return 0;
    }
    close(fd); /* unwind from error */
  }

  free(ptr); /* unwind */

  return -1;
}

