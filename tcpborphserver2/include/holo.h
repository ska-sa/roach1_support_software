#ifndef HOLO_POCO_H_
#define HOLO_POCO_H_

#include <stdint.h>
#include <time.h>

#include <sys/time.h>

#include "holo-config.h"
#include "core.h"
#include "poco.h"

#define DATA_OVERHEAD_HOLO    ((3 + 1) * 8)
#define PACKET_OVERHEAD_POCO (14 + 20 + 16)
#define MTU_POCO                      1500

#define PI_MICRO_POCO              3141592

#define HWMON_COUNT_POCO                 4

  
struct capture_holo{
    unsigned int c_magic;
    char *c_name;
    int c_fd;

    uint16_t c_port;
    uint32_t c_ip;

    struct timeval c_prep;
    struct timeval c_start;
    struct timeval c_stop;

    int c_state;/*TO DO:Check this*/

    int (*c_schedule)(struct katcp_dispatch *d, struct capture_holo *ch, int poke);

    /*Not of much use found;option set as debug in meta_udp_poco(udp.c)*/
    uint16_t c_ts_msw;/*TIMESTAMP_OPTION_POCO*/
    uint32_t c_ts_lsw;
    /*imp;gets set each time a packet is xmitted*/
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


struct state_holo{
  char *h_image;

  struct capture_holo **h_captures;
  int h_size;

  uint32_t h_source_state;
  unsigned int h_accumulation_length;
  unsigned int h_center_freq;
  unsigned int h_time_stamp;

  struct timeval h_sync_time;
  unsigned int h_lead;

  unsigned int h_dsp_clock;
  unsigned int h_scale_factor;

  unsigned int h_fft_window;

  unsigned int h_dump_count; /* multiply by p_pre_accumulation to get the accumlations */

  int h_manual;/*no pps available*/

  unsigned int inp_select;/*adc bram input select*/
  struct ntp_sensor_poco h_ntp_sensor;

};

struct snap_bram{
    unsigned int b_magic;
    struct capture_holo *b_capture;
    struct poco_core_entry **b_bram;
    uint32_t *b_buffer;
    unsigned int b_blocks;
    unsigned int b_size;   

    struct poco_core_entry *b_ctl;
    struct poco_core_entry *b_sts;
    struct poco_core_entry *b_stamp;

    unsigned int b_got;
    unsigned int b_lost;
    unsigned int b_txp;
    uint32_t b_ts_old;
    unsigned int b_first_time;
    struct timeval b_now;
    struct timeval b_old;
};

int setup_holo_poco(struct katcp_dispatch *d, char *image);

/*capture support logic*/
#define CAPTURE_POKE_AUTO   0
#define CAPTURE_POKE_START  1
#define CAPTURE_POKE_STOP   2

struct capture_holo *find_capture_holo(struct state_holo *sh, char *name);
int register_capture_holo(struct katcp_dispatch *d, char *name, int (*call)(struct katcp_dispatch *d, struct capture_holo *ch, int poke));
void destroy_capture_holo(struct katcp_dispatch *d, struct capture_holo *ch);


/*udp logic*/
int init_udp_holo(struct katcp_dispatch *d, struct capture_holo *ch);
void *data_udp_holo(struct katcp_dispatch *d, struct capture_holo *ch, unsigned int offset, unsigned int len);
int meta_udp_holo(struct katcp_dispatch *d, struct capture_holo *ch, int control);
int tx_udp_holo(struct katcp_dispatch *d, struct capture_holo *ch);

/* commands defined outside holo.c */
int holo_snap_shot_cmd(struct katcp_dispatch *d, int argc);

/*bram logic*/
int create_bram_holo(struct katcp_dispatch *d, struct capture_holo *ch, unsigned int blocks);
int register_bram_holo(struct katcp_dispatch *d, char *name, unsigned int count);
void destroy_bram_holo(struct katcp_dispatch *d, void *data);
int run_bram_holo(struct katcp_dispatch *d, void *data);

/*katadc logic*/
int katadc_init(struct katcp_dispatch *d, uint8_t slot);
#ifdef KATADC
int katadc_config_cmd(struct katcp_dispatch *d, int argc);
#endif

#endif
