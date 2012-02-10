
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <loop.h>
#include <katcp.h>
#include <katcl.h>

#include "multiserver.h"

struct mul_msg *create_msg(void)
{
  struct mul_msg *mm;

  mm = malloc(sizeof(struct mul_msg));
  if(mm == NULL){
    return NULL;
  }

  mm->m_buffer = NULL;
  mm->m_used = 0;
  mm->m_space = 0;

  mm->m_total = 0;
  mm->m_have = 0;
  mm->m_table = NULL;

  mm->m_tv.tv_sec = 5;
  mm->m_tv.tv_usec = 0;

  mm->m_tag = 0;

  return mm;
}

void release_msg(struct list_loop *ls, void *datum)
{
  destroy_msg(datum);
}

void destroy_msg(struct mul_msg *mm)
{
  if(mm == NULL){
    return;
  }

  if(mm->m_buffer){
    free(mm->m_buffer);
    mm->m_buffer = NULL;
  }
  mm->m_used = 0;
  mm->m_space = 0;

  if(mm->m_table){
    free(mm->m_table);
    mm->m_table = NULL;
  }
  mm->m_total = 0;
  mm->m_have = 0;

  free(mm);
}

void empty_msg(struct mul_msg *mm)
{
  if(mm->m_table){
    mm->m_table[0].i_start = 0;
    mm->m_table[0].i_length = 0;
  }

  mm->m_space += mm->m_used;
  mm->m_used = 0;

  mm->m_have = 0;

  mm->m_tv.tv_sec = 5;
  mm->m_tv.tv_usec = 0;

  mm->m_tag = 0;
}

unsigned int get_tag_msg(struct mul_msg *mm)
{
  return mm ? mm->m_tag : 0;
}

void set_tag_msg(struct mul_msg *mm, unsigned int tag)
{
  if(mm){
    mm->m_tag = tag;
  }
}

int truncate_msg(struct mul_msg *mm, unsigned int count)
{
  unsigned int used;
  int i;

  if(mm->m_have < count){
    return 1;
  }

  mm->m_have = count;
  used = 0;

  for(i = 0; i < count; i++){
    used += mm->m_table[i].i_length;
  }

  if(used > mm->m_used){
#ifdef DEBUG
    fprintf(stderr, "msg: logic failure: claims to use %u, actually does %u\n", mm->m_used, used);
    abort();
#endif
    return -1;
  }

  mm->m_space -= (mm->m_used - used);
  mm->m_used = used;

  if(mm->m_table && (mm->m_have < mm->m_total)){
    mm->m_table[mm->m_have].i_start = mm->m_used;
    mm->m_table[mm->m_have].i_length = 0;
  }

  return 0;
}

char *reserve_msg(struct mul_msg *mm, int space)
{
  char *tmp;

  if(mm->m_space > space){
    return mm->m_buffer + mm->m_used;
  }

  tmp = realloc(mm->m_buffer, mm->m_used + space + 1);
  if(tmp == NULL){
    return NULL;
  }

  mm->m_buffer = tmp;
  mm->m_space = space + 1;

  return mm->m_buffer + mm->m_used;
}

int append_more_msg(struct mul_msg *mm, int length, int more)
{
  struct mul_arg *ma;
  char *tmp;

  /* WARNING: scary logic changes, i_start field now set much earlier (in previous complete append) */

  /* if we are asked to save something, there better be a previous request which allocated the space */
  if((length > 0) && (mm->m_space <= length)){ 
    return -1;
  }

  if(mm->m_have >= mm->m_total){
    ma = realloc(mm->m_table, sizeof(struct mul_arg) * (mm->m_have + 1));
    if(ma == NULL){
      return -1;
    }
    mm->m_table = ma;
    mm->m_total = mm->m_have + 1;

    mm->m_table[mm->m_have].i_start = mm->m_used;
    mm->m_table[mm->m_have].i_length = 0;
  }

  mm->m_table[mm->m_have].i_length += length;

  mm->m_used += length;
  mm->m_space -= length;

  if(more == 0){

    if((mm->m_buffer == NULL) && (mm->m_space > 0)){
#ifdef DEBUG
      fprintf(stderr, "append: major logic failure: claim to have space but no buffer\n");
      abort();
#endif
      return -1;
    }

    if(mm->m_space <= 0){
      tmp = realloc(mm->m_buffer, mm->m_used + 1);
      if(tmp == NULL){
        return -1;
      }
      mm->m_buffer = tmp;
      mm->m_space  = 1;
    }

    if((mm->m_used <= 0) || (mm->m_buffer[mm->m_used - 1] != '\0')){
      mm->m_buffer[mm->m_used] = '\0';
      mm->m_used++;
    }

    mm->m_have++;

    if(mm->m_have < mm->m_total){
      mm->m_table[mm->m_have].i_start = mm->m_used;
      mm->m_table[mm->m_have].i_length = 0;
    }
  }

  return length;
}

int append_msg(struct mul_msg *mm, int length)
{
  return append_more_msg(mm, length, 0);

#if  0
  struct mul_arg *ma;

  if(mm->m_space < length){
    return -1;
  }

  if(mm->m_have >= mm->m_total){
    ma = realloc(mm->m_table, sizeof(struct mul_arg) * (mm->m_have + 1));
    if(ma == NULL){
      return -1;
    }
    mm->m_table = ma;
    mm->m_total = mm->m_have + 1;
  }

  mm->m_table[mm->m_have].i_start = mm->m_used;
  mm->m_table[mm->m_have].i_length = length;

  mm->m_used += length;
  mm->m_space -= length;

  mm->m_have++;

  return 0;
#endif
}

/************************************/

int arg_type_msg(struct mul_msg *mm, char mode)
{
  if(mm->m_have <= 0){
    return 0;
  }

#ifdef DEBUG
  if((mm->m_table == NULL) || (mm->m_buffer == NULL)){
    fprintf(stderr, "msg arg: major logic failure, no buffers allocated\n");
    abort();
  }
  if(mm->m_table[0].i_start != 0){
    fprintf(stderr, "msg arg: major logic failure, first element should start at 0\n");
    abort();
  }
#endif

  if(mm->m_buffer[0] == mode){
    return 1;
  }

  return 0;
}

int arg_request_msg(struct mul_msg *mm)
{
  return arg_type_msg(mm, KATCP_REQUEST);
}

int arg_reply_msg(struct mul_msg *mm)
{
  return arg_type_msg(mm, KATCP_REPLY);
}

int arg_inform_msg(struct mul_msg *mm)
{
  return arg_type_msg(mm, KATCP_INFORM);
}

unsigned int arg_count_msg(struct mul_msg *mm)
{
  return mm->m_have;
}

int arg_null_msg(struct mul_msg *mm, unsigned int index)
{
  if(index >= mm->m_have){
    return 1;
  } 

  if(mm->m_table[index].i_length <= 0){
    return 1;
  }

  return 0;
}

char *arg_string_msg(struct mul_msg *mm, unsigned int index)
{
  if(index >= mm->m_have){
    return NULL;
  } 

  if(mm->m_table[index].i_length <= 0){
    return NULL;
  }

  return mm->m_buffer + mm->m_table[index].i_start;
}

char *arg_copy_string_msg(struct mul_msg *mm, unsigned int index)
{
  char *ptr;

  ptr = arg_string_msg(mm, index);
  if(ptr){
    return strdup(ptr);
  } else {
    return NULL;
  }
}

unsigned long arg_unsigned_long_msg(struct mul_msg *mm, unsigned int index)
{
  unsigned long value;

  if(index >= mm->m_have){
    return 0;
  } 

  value = strtoul(mm->m_buffer + mm->m_table[index].i_start, NULL, 0);

  return value;
}

unsigned int arg_buffer_msg(struct mul_msg *mm, unsigned int index, void *buffer, unsigned int size)
{
  unsigned int want, done;

  if(index >= mm->m_have){
    return 0;
  } 

  want = mm->m_table[index].i_length;
  done = (want > size) ? size : want;

  if(buffer && (want > 0)){
    memcpy(buffer, mm->m_buffer + mm->m_table[index].i_start, done);
  }

  return want;
}

/************************************/

int append_buffer_msg(struct mul_msg *mm, int flags, void *buffer, int len)
{
  /* returns greater than zero on success */
  unsigned int want, problem;
  char *s, *ptr;

  problem = KATCP_FLAG_MORE | KATCP_FLAG_LAST;
  if((flags & problem) == problem){
#ifdef DEBUG
    fprintf(stderr, "append: usage problem: can not have last and more together\n");
#endif
    return -1;
  }

  if(len < 0){
    return -1;
  }

  s = buffer;
  want = s ? len : 0;

  /* extra checks */
  if(flags & KATCP_FLAG_FIRST){
    if(s == NULL){
      return -1;
    }
    switch(s[0]){
      case KATCP_INFORM  :
      case KATCP_REPLY   :
      case KATCP_REQUEST :
        break;
      default :
        return -1;
    }
  }

  if(want){ /* non empty string wants buffer space allocated */
    ptr = reserve_msg(mm, want);
    if(ptr == NULL){
      return -1;
    }

    memcpy(ptr, s, want);
  }

  if(append_more_msg(mm, want, (flags & KATCP_FLAG_MORE) ? 1 : 0) < 0){
    return -1;
  }

  /* TODO: maybe not ignore KATCP_FLAG_LAST, tag message as complete */

  return 0;
}

int append_string_msg(struct mul_msg *mm, int flags, char *buffer)
{
  return append_buffer_msg(mm, flags, buffer, strlen(buffer));
}

int append_unsigned_long_msg(struct mul_msg *mm, int flags, unsigned long v)
{
#define TMP_BUFFER 32
  char buffer[TMP_BUFFER];
  int result;

  result = snprintf(buffer, TMP_BUFFER, "%lu", v);
  if((result <= 0) || (result >= TMP_BUFFER)){
    return -1;
  }

  return append_buffer_msg(mm, flags, buffer, result);
#undef TMP_BUFFER
}

int append_hex_long_msg(struct mul_msg *mm, int flags, unsigned long v)
{
#define TMP_BUFFER 32
  char buffer[TMP_BUFFER];
  int result;

  result = snprintf(buffer, TMP_BUFFER, "0x%lx", v);
  if((result <= 0) || (result >= TMP_BUFFER)){
    return -1;
  }

  return append_buffer_msg(mm, flags, buffer, result);
#undef TMP_BUFFER
}

int vsend_msg(struct mul_msg *mm, va_list ap)
{
  int flags, result, check;
  char *string;
  void *buffer;
  unsigned long value;
  int len;

  check = KATCP_FLAG_FIRST;

  empty_msg(mm);
  
  do{
    flags = va_arg(ap, int);
    if((check & flags) != check){
      /* WARNING: tests first arg for FLAG_FIRST */
      return -1;
    }
    check = 0;

    switch(flags & KATCP_TYPE_FLAGS){
      case KATCP_FLAG_STRING :
        string = va_arg(ap, char *);
        result = append_string_msg(mm, flags & KATCP_ORDER_FLAGS, string);
        break;
      case KATCP_FLAG_ULONG :
        value = va_arg(ap, unsigned long);
        result = append_unsigned_long_msg(mm, flags & KATCP_ORDER_FLAGS, value);
        break;
      case KATCP_FLAG_XLONG :
        value = va_arg(ap, unsigned long);
        result = append_hex_long_msg(mm, flags & KATCP_ORDER_FLAGS, value);
        break;
      case KATCP_FLAG_BUFFER :
        buffer = va_arg(ap, void *);
        len = va_arg(ap, int);
        result = append_buffer_msg(mm, flags & KATCP_ORDER_FLAGS, buffer, len);
        break;
      default :
        result = (-1);
    }
#ifdef DEBUG
    fprintf(stderr, "vsend: appended: flags=0x%02x, result=%d\n", flags, result);
#endif
    if(result <= 0){
      return -1;
    }
  } while(!(flags & KATCP_FLAG_LAST));

  return 0;
}

int send_msg(struct mul_msg *mm, ...)
{
  int result;
  va_list ap;

  va_start(ap, mm);

  result = vsend_msg(mm, ap);

  va_end(ap);

  return result;
}

/************************************/

int line_to_msg(struct katcl_line *l, struct mul_msg *mm)
{
  unsigned int i, m, z;
  char *ptr;

  empty_msg(mm);

  m = arg_count_katcl(l);
  for(i = 0; i < m; i++){
    z = arg_buffer_katcl(l, i, NULL, 0);
    ptr = reserve_msg(mm, z);
    if(ptr == NULL){
      return -1;
    }
    if(z != arg_buffer_katcl(l, i, ptr, z)){
      return -1;
    }
    if(append_msg(mm, z) < 0){
      return -1;
    }
  }

  return 0;
}

int line_to_dispatch(struct katcp_dispatch *d, struct mul_msg *mm)
{
  struct katcl_line *l;

  l = line_katcp(d);
  if(l == NULL){
    return -1;
  }

  return line_to_msg(l, mm);
}

/************************************/

int line_from_msg(struct katcl_line *l, struct mul_msg *mm)
{
  struct mul_arg *ma;
  int flags, i;
  unsigned int size;
  char *ptr;

  flags = KATCP_FLAG_FIRST | KATCP_FLAG_BUFFER;

  i = 0;
  while(i < mm->m_have){
    ma = &(mm->m_table[i]);
    ptr = mm->m_buffer + ma->i_start;
    size = ma->i_length;

    i++;
    if(i == mm->m_have){
      flags |= KATCP_FLAG_LAST;
    }
    if(append_buffer_katcl(l, flags, ptr, size) < 0){
      return -1;
    }
    flags = KATCP_FLAG_BUFFER;
  }

  return 0;
}

int dispatch_from_msg(struct katcp_dispatch *d, struct mul_msg *mm)
{
  struct katcl_line *l;

  l = line_katcp(d);
  if(l == NULL){
    return -1;
  }

  return line_from_msg(l, mm);
}

int line_is_reply_msg(struct katcl_line *l, struct mul_msg *mm)
{
  char *strl, *strm;

  strl = arg_string_katcl(l, 0);
  if(strl == NULL){
    return -1;
  }

  strm = arg_string_msg(mm, 0);
  if(strm == NULL){
    return -1;
  }

  if(strm[0] != KATCP_REQUEST){
    return -1;
  }

  if(strl[0] != KATCP_REPLY){
    return 0;
  }  

  return (strcmp(strl + 1, strm + 1) == 0) ? 1 : 0;
}

int make_fail_reply_msg(struct mul_msg *mm)
{
  char *ptr;
  int len;

  ptr = mm->m_buffer;
  if(ptr == NULL){
#ifdef DEBUG
    fprintf(stderr, "fail: no payload buffer in message\n");
#endif
    return -1;
  }

  if(ptr[0] != KATCP_REQUEST){
#ifdef DEBUG
    fprintf(stderr, "fail: not a request, but <%c>\n", ptr[0]);
#endif
    return -1;
  }

  ptr[0] = KATCP_REPLY;

  if(truncate_msg(mm, 1)){
#ifdef DEBUG
    fprintf(stderr, "fail: unable to truncate request\n");
#endif
    return -1;
  }

  len = strlen(KATCP_FAIL);
  ptr = reserve_msg(mm, len);
  if(ptr == NULL){
#ifdef DEBUG
    fprintf(stderr, "fail: unable to reserve %d bytes\n", len);
#endif
    return -1;
  }

  memcpy(ptr, KATCP_FAIL, len);
  if(append_msg(mm, len) < 0){
#ifdef DEBUG
    fprintf(stderr, "fail: unable to append <%s> (%d) bytes\n", KATCP_FAIL, len);
#endif
    return -1;
  }

  return 0;
}

#ifdef DEBUG

void dump_msg(struct mul_msg *mm, FILE *fp)
{
  int i, j;
  unsigned int count;
  char *ptr;

  count = arg_count_msg(mm);

  fprintf(fp, "msg:");
  for(i = 0; i < count; i++){
    fprintf(fp, " [%d]=", i);
    for(j = 0; j < mm->m_table[i].i_length; j++){
      fputc(mm->m_buffer[mm->m_table[i].i_start + j], fp);
    }
  }
  fprintf(fp, " (spare: bytes=%u, entries=%d)\n", mm->m_space, mm->m_total - mm->m_have);

  for(i = 0; i < count; i++){
    ptr = arg_string_msg(mm, i);
    if(ptr){
      fprintf(fp, "[%d]=<%s>\n", i, ptr);
    }
  }

}
#endif

#ifdef UNIT_TEST_MSG
#define BUFFER 512

int main()
{
  char buffer[BUFFER];
  struct mul_msg *mm;
  int len;
  char *ptr;

  mm = create_msg();
  if(mm == NULL){
    fprintf(stderr, "unable to allocate message\n");
    return 1;
  }

  while(fgets(buffer, BUFFER, stdin)){
    ptr = strchr(buffer, '\r');
    if(ptr) *ptr = '\0';
    ptr = strchr(buffer, '\n');
    if(ptr) *ptr = '\0';

    len = strlen(buffer);
    if(len <= 0){
      dump_msg(mm, stderr);
      empty_msg(mm);
    } else {
      ptr = reserve_msg(mm, len); 
      if(ptr){
        memcpy(ptr, buffer, len);
        append_msg(mm, len);
      }
    }
  }

  release_msg(NULL, mm);

  return 0;
}
#endif
