#ifndef CORE_POCO_H_
#define CORE_POCO_H_

#include <unistd.h>
#include <sys/types.h>

#include <katcp.h>

#define POCO_CLIENT_LIMIT 16

#define POCO_LOADTIME      4

#define POCO_CORE_BORPH   80

struct poco_core_entry
{
  int e_fd;
  unsigned int e_size;
  char *e_name;
  char *e_full_name;
  int e_seek;
};

struct poco_core_state
{
  unsigned int c_magic;

  int c_borph_fd;
  char c_borph_proc[POCO_CORE_BORPH];
  char *c_borph_image;
#if 0
  pid_t c_borph_pid;
#endif
  int c_real;

  struct poco_core_entry *c_table;
  unsigned int c_size;
};

int setup_core_poco(struct katcp_dispatch *d, char *fake);
void destroy_core_poco(struct katcp_dispatch *d);

int program_core_poco(struct katcp_dispatch *d, char *image);
int programmed_core_poco(struct katcp_dispatch *d, char *image);

/* poco core entries (pce) */

char *name_pce(struct poco_core_entry *pce);
char *full_name_pce(struct poco_core_entry *pce);

int insert_pce(struct katcp_dispatch *d, char *name);
int clear_all_pce(struct katcp_dispatch *d);

struct poco_core_entry *by_name_pce(struct katcp_dispatch *d, char *name);
struct poco_core_entry *by_offset_pce(struct katcp_dispatch *d, unsigned int offset);

int read_pce(struct katcp_dispatch *d, struct poco_core_entry *pce, void *buffer, unsigned int start, unsigned int length);
int write_pce(struct katcp_dispatch *d, struct poco_core_entry *pce, void *buffer, unsigned int start, unsigned int length);

int read_name_pce(struct katcp_dispatch *d, char *name, void *buffer, unsigned int start, unsigned int length);
int write_name_pce(struct katcp_dispatch *d, char *name, void *buffer, unsigned int start, unsigned int length);

#endif
