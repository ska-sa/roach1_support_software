#ifndef POCO_POCO_H_
#define POCO_POCO_H_

#include <stdint.h>
#include <time.h>

#include <sys/time.h>

#include "config.h"
#include "core.h"

#define DATA_OVERHEAD_POCO    ((3 + 1) * 8)
#define PACKET_OVERHEAD_POCO (14 + 20 + 16)
#define MTU_POCO                      1500

#define HWMON_COUNT_POCO                 4

struct capture_poco;

struct fixer_poco{
  unsigned int f_magic;
  int f_id;
  unsigned int f_up;
  unsigned int f_fresh;

  uint64_t f_this_mcount;  /* the next mcount at which register to be loaded */
  unsigned int f_this_add; /* how much to add to mcount each time */

  int f_this_delta; /* amount by which delay changes, in cycles */
  int f_this_delay; /* current delay, updated by delta */

  uint64_t f_stage_mcount; /* the mcount at which we cut over to the stage items */
  unsigned int f_stage_add;

  int f_stage_delta;
  int f_stage_delay;

  int f_phase_mr; /* phase in milli-radians */

  struct poco_core_entry *f_next_lsb;
  struct poco_core_entry *f_next_msb;
  struct poco_core_entry *f_next_value;
  struct poco_core_entry *f_control;
  struct poco_core_entry *f_status;
};

struct snap_poco{
  unsigned int n_magic;
  struct capture_poco *n_capture;
  struct poco_core_entry *n_ctrl;
  struct poco_core_entry *n_addr;
  struct poco_core_entry **n_bram;
  unsigned int n_blocks;
  unsigned int n_count;
  unsigned int n_chunk;
  unsigned int n_state;
  char *n_name;
  unsigned int n_pos;
};

struct dram_poco{
  unsigned int r_magic;
  struct capture_poco *r_capture;
  struct poco_core_entry *r_ctl;
  struct poco_core_entry *r_sts;
  struct poco_core_entry *r_dram;
  struct poco_core_entry *r_stamp;
  unsigned int r_count;
  unsigned int r_chunk;
  unsigned int r_state;
  unsigned int r_number;

  unsigned int r_lost;
  unsigned int r_got;
  unsigned int r_txp;
  unsigned int r_nul;
  
  unsigned char *r_buffer;
  unsigned int r_size;
};

struct capture_poco{
  unsigned int c_magic;
  char *c_name;
  int c_fd;

  uint16_t c_port;
  uint32_t c_ip;

  struct timeval c_prep;
  struct timeval c_start;
  struct timeval c_stop;

#if 0
  struct timeval c_period;
#endif
  int c_state;
#if 0
  int c_ping;
#endif

  int (*c_schedule)(struct katcp_dispatch *d, struct capture_poco *cp, int poke);

  uint16_t c_ts_msw;
  uint32_t c_ts_lsw;
  unsigned int c_options;

  unsigned char *c_buffer;
  unsigned int c_size;   /* how large is the buffer */
  unsigned int c_sealed; /* no more options, only add data at sealed offset */
  unsigned int c_limit;  /* max size */
  unsigned int c_used;   /* how much contains data */

  unsigned int c_failures;

  void *c_dump;
  int (*c_toggle)(struct katcp_dispatch *d, void *data, int start);
  void (*c_destroy)(struct katcp_dispatch *d, void *data);
};

struct overflow_sensor_poco
{
  struct poco_core_entry *a_status;
  struct poco_core_entry *a_control;
};

struct hwmon_sensor_poco
{
  int h_fd;
  int h_least;
  int h_most;
};

struct ntp_sensor_poco
{
  unsigned int n_magic;
  int n_fd;
  unsigned int n_sequence; /* careful, field in packet is only 16 bits */
};

struct state_poco{
  char *p_image;

  struct capture_poco **p_captures;
  int p_size;
  struct timeval p_sync_time;
  unsigned int p_lead; /* lead time in micro seconds */

  struct fixer_poco *p_fixers[POCO_ANT_COUNT * POCO_POLARISATION_COUNT];

  unsigned int p_dsp_clock;
  unsigned int p_fft_window;
  unsigned int p_pre_accumulation;

  unsigned int p_accumulation_length; /* in ms */
  uint32_t p_dump_count; /* multiply by p_pre_accumulation to get the accumlations */
  uint32_t p_sync_period;
  uint32_t p_source_state;

  uint64_t p_mcount;

  int p_manual; /* no pps available */

  struct overflow_sensor_poco p_overflow_sensor;
  struct hwmon_sensor_poco p_hwmon_sensor[HWMON_COUNT_POCO];
  struct ntp_sensor_poco p_ntp_sensor;
};

struct state_poco *ready_poco_poco(struct katcp_dispatch *d);
int setup_poco_poco(struct katcp_dispatch *d, char *image);

/* capture support stuff */

#define CAPTURE_POKE_AUTO   0
#define CAPTURE_POKE_START  1
#define CAPTURE_POKE_STOP   2

struct capture_poco *find_capture_poco(struct state_poco *sp, char *name);

int register_capture_poco(struct katcp_dispatch *d, char *name, int (*call)(struct katcp_dispatch *d, struct capture_poco *cp, int poke));
void destroy_capture_poco(struct katcp_dispatch *d, struct capture_poco *cp);

#if 0
int run_test_capture(struct katcp_dispatch *d, void *data);
int run_generic_capture(struct katcp_dispatch *d, void *data);
#endif

int prepare_accumulator_poco(struct katcp_dispatch *d);
void time_accumulator_poco(struct katcp_dispatch *d, struct timeval *tv);

/* udp logic */
int init_udp_poco(struct katcp_dispatch *d, struct capture_poco *cp);
void *data_udp_poco(struct katcp_dispatch *d, struct capture_poco *cp, unsigned int offset, unsigned int len);
int meta_udp_poco(struct katcp_dispatch *d, struct capture_poco *cp, int control);
int tx_udp_poco(struct katcp_dispatch *d, struct capture_poco *cp);

/* fixer logic */
struct fixer_poco *startup_fixer(struct katcp_dispatch *d, int instance);
void shutdown_fixer(struct katcp_dispatch *d, struct fixer_poco *fp);

/* commands defined outside poco.c */
int poco_gain_cmd(struct katcp_dispatch *d, int argc);
int poco_snap_shot_cmd(struct katcp_dispatch *d, int argc);

/* snap logic */
int create_snap_poco(struct katcp_dispatch *d, struct capture_poco *cp, char *name, int count, unsigned int blocks);
int register_snap_poco(struct katcp_dispatch *d, char *name, char *prefix, int count, unsigned int blocks);

/* dram logic */
int create_dram_poco(struct katcp_dispatch *d, struct capture_poco *cp, unsigned int count);
int register_dram_poco(struct katcp_dispatch *d, char *name, unsigned int count);

/* fixer logic */
int schedule_fixer(struct katcp_dispatch *d, struct fixer_poco *fp);

/* mcount supporting stuff - uses 64bits - argh */
int mcount_to_tv_poco(struct katcp_dispatch *d, struct timeval *tv, uint64_t *count);
int tve_to_mcount_poco(struct katcp_dispatch *d, uint64_t *count, struct timeval *tv, unsigned int extra);

/* hwmon sensors */
int setup_hwmon_poco(struct katcp_dispatch *d, struct hwmon_sensor_poco *hp, char *label, char *description, char *file, int least, int most);

/* overflow sensors */
int acquire_overflow_poco(struct katcp_dispatch *d, void *data);
int extract_adc_poco(struct katcp_dispatch *d, struct katcp_sensor *sn);
int extract_quant_poco(struct katcp_dispatch *d, struct katcp_sensor *sn);
int extract_delay_poco(struct katcp_dispatch *d, struct katcp_sensor *sn);

/* ntp sensor */
void clear_ntp_poco(struct katcp_dispatch *d, struct ntp_sensor_poco *nt);
int init_ntp_poco(struct katcp_dispatch *d, struct ntp_sensor_poco *nt);
int acquire_ntp_poco(struct katcp_dispatch *d, void *data);

#endif
