#ifndef MULTI_SERVER_H_
#define MULTI_SERVER_H_

#include <katcp.h>
#include <loop.h>

#ifndef SUBSYSTEM
#define SUBSYSTEM "multiserver"
#endif

#ifndef MAJOR
#define MAJOR 0
#endif

#ifndef MINOR
#define MINOR 1
#endif

#define GLOBAL_TASK_LIMIT  64
#define GLOBAL_LINK_LIMIT 128

#define CLIENT_NAME "katcp-client"
#define CLIENT_TASK_LIMIT  16

#define INTER_NAME  "katcp-inter"
#define INTER_TASK_LIMIT   16

#define LEAF_NAME   "katcp-leaf"
#define LEAF_TASK_LIMIT    16

#define MSG_NAME    "katcp-msg"
#define MSG_TASK_LIMIT     16

#define MUL_MAGIC 0xf235f0a9

/* the state for a client connection running inside a multiserver */
struct mul_client
{
#ifdef DEBUG
  int c_magic;
#endif

  struct katcp_dispatch *c_dispatch;

  struct task_loop *c_task;
  struct list_loop *c_loop;

  struct overall_state *c_overall;

  struct task_loop *c_save; /* brief save, may need to clear */

  struct xlookup_state *c_xlookup;

  int c_queue;  /* how many requests are outstanding ? */
  int c_waiting;
};

#define LEAF_IDLE     0
#define LEAF_WAITING  1

/* state for leaf relay, the connection to a subordinate node */
struct mul_leaf
{
  struct katcl_line *l_line;
  int l_fresh;
  char *l_name;
  char *l_version;
  int l_idle;
};

/* used by mul message */
struct mul_arg{
  unsigned int i_start;
  unsigned int i_length;
};

/* the data exchanged between client connections and subordinate nodes */
struct mul_msg{
  char *m_buffer;
  unsigned int m_used;
  unsigned int m_space;

  unsigned int m_have;
  unsigned int m_total;
  struct mul_arg *m_table;

  struct timeval m_tv;
  unsigned int m_tag; /* WARNING: presupposes that msgs get re-used */
};

/* the global state, just hold ids for loop library */
struct overall_state{
  int o_type_client;
  int o_type_inter;
  int o_type_leaf;
  int o_type_msg;
};

#define XLK_FLAG_COMPLETE  0x1 /* wait until all outstanding requests done */
#define XLK_FLAG_CLASS     0x2 /* target is a class of entities */

#define XLK_LABEL_OK       (-1) /* finish, successfully */
#define XLK_LABEL_FAIL     (-2) /* finish, with failure */

/* an entry in the state transition table */
struct xlookup_entry{
  char *x_target;
  int (*x_issue)(struct mul_client *ci, struct mul_msg *mm);
  int (*x_collect)(struct mul_client *ci, struct mul_msg *mm);

  unsigned int x_flags; /* currently unused */

  int x_success;   /* transition if fn returns > 0 */
  int x_failure;  /* transition if fn returns < 0 */
};

/* the state added to the multitserver state when using the lookup table */
struct xlookup_state{
  struct xlookup_entry *s_table;
  int s_size;
  int s_current;

  int s_status; /* what is the total status */
  int s_repeats; /* how many repeats for this function */
};

void shutdown_client(struct mul_client *ci);
struct mul_client *create_client(struct list_loop *ls);
void *setup_client(struct list_loop *ls, void *data, struct sockaddr *addr, int fd);
int run_client(struct list_loop *ls, struct task_loop *tl, int mask);

struct task_loop *start_leaf(struct list_loop *ls, char *remote);

/* commands */
int busy_cmd(struct katcp_dispatch *d, int argc);
int leaf_connect_cmd(struct katcp_dispatch *d, int argc);
int leaf_relay_cmd(struct katcp_dispatch *d, int argc);
int leaf_list_cmd(struct katcp_dispatch *d, int argc);

int x_test_cmd(struct katcp_dispatch *d, int argc);

/* message handling */
struct mul_msg *create_msg(void);
char *reserve_msg(struct mul_msg *mm, int space);
int append_msg(struct mul_msg *mm, int length);
void empty_msg(struct mul_msg *mm);
int truncate_msg(struct mul_msg *mm, unsigned int count);

unsigned int get_tag_msg(struct mul_msg *mm);
void set_tag_msg(struct mul_msg *mm, unsigned int tag);

void destroy_msg(struct mul_msg *mm);
void release_msg(struct list_loop *ls, void *datum);

/* conversion between protocol buffer and message */
int line_to_msg(struct katcl_line *l, struct mul_msg *mm);
int line_from_msg(struct katcl_line *l, struct mul_msg *mm);
int dispatch_to_msg(struct katcp_dispatch *d, struct mul_msg *mm);
int dispatch_from_msg(struct katcp_dispatch *d, struct mul_msg *mm);
int line_is_reply_msg(struct katcl_line *l, struct mul_msg *mm);
int make_fail_reply_msg(struct mul_msg *mm);
/* API could be expanded to have dispatch_is_reply_msg */

/* API looks familiar, v similar to katcp one */
int arg_request_msg(struct mul_msg *mm);
int arg_reply_msg(struct mul_msg *mm);
int arg_inform_msg(struct mul_msg *mm);
unsigned int arg_count_msg(struct mul_msg *mm);
int arg_null_msg(struct mul_msg *mm, unsigned int index);
char *arg_string_msg(struct mul_msg *mm, unsigned int index);
char *arg_copy_string_msg(struct mul_msg *mm, unsigned int index);
unsigned long arg_unsigned_long_msg(struct mul_msg *mm, unsigned int index);
unsigned int arg_buffer_msg(struct mul_msg *mm, unsigned int index, void *buffer, unsigned int size);
int append_buffer_msg(struct mul_msg *mm, int flags, void *buffer, int len);
int append_string_msg(struct mul_msg *mm, int flags, char *buffer);
int append_unsigned_long_msg(struct mul_msg *mm, int flags, unsigned long v);
int append_hex_long_msg(struct mul_msg *mm, int flags, unsigned long v);

/* for multiserver, allows us to talk to other nodes */
struct mul_msg *issue_katmc(struct katcp_dispatch *d, char *target);
struct mul_msg *collect_katmc(struct katcp_dispatch *d);
void release_katmc(struct katcp_dispatch *d, struct mul_msg *mm);
int outstanding_katmc(struct katcp_dispatch *d);

struct mul_msg *issue_katic(struct mul_client *ci, char *target);
struct mul_msg *collect_katic(struct mul_client *ci);
void release_katic(struct mul_client *ci, struct mul_msg *mm);
int outstanding_katic(struct mul_client *ci);

int enter_client_xlookup(struct katcp_dispatch *d, struct xlookup_entry *table);
int start_inter_xlookup(struct list_loop *ls, struct xlookup_entry *table);

/* yet another layer, helps set up clients with state machine */
struct xlookup_state *create_xlookup(void);
void destroy_xlookup(struct xlookup_state *xs);
int prepare_xlookup(struct xlookup_state *xs, struct xlookup_entry *table, char *extra);
int run_xlookup(struct mul_client *ci, int argc);
int generic_collect_xlookup(struct mul_client *ci, struct mul_msg *mm);

#endif
