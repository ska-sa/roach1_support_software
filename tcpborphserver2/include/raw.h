#ifndef RAW_POCO_H_
#define RAW_POCO_H_

#define RAW_TYPE_TGTAP 1
#define WAIT_END_TGTAP 1

struct state_raw{
  char *s_bof_path;
  int s_align_check;
};

/* raw mode registration routine in raw.c */
int setup_raw_poco(struct katcp_dispatch *d, char *bofs, int check);

/* echo test in echo.c */
int echotest_cmd(struct katcp_dispatch *d, int argc);

/* uploading of bof files in upload.c */
int uploadbof_cmd(struct katcp_dispatch *d, int argc);

int word_write_cmd(struct katcp_dispatch *d, int argc);
int word_read_cmd(struct katcp_dispatch *d, int argc);

/* tgtap.c */
int stop_tgtap_cmd(struct katcp_dispatch *d, int argc);
int start_tgtap_cmd(struct katcp_dispatch *d, int argc);

#endif
