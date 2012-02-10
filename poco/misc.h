#ifndef MISC_POCO_H_
#define MISC_POCO_H_

#include <katcp.h>

/* misc support */
int display_dir_poco_cmd(struct katcp_dispatch *d, char *path);
int time_from_string(struct timeval *tv, unsigned int *femto, char *str);
int shift_point_string(long *value, char *buffer, unsigned int shift);

#define ANT_EXTRACT_POCO 0
#define POLARISATION_EXTRACT_POCO 1
#define CHANNEL_EXTRACT_POCO 2

int extract_field_poco(char *string, int what);

#define extract_input_poco extract_ant_poco

int extract_ant_poco(char *string);
int extract_polarisation_poco(char *string);
int extract_channel_poco(char *string);

#define MSECPERSEC_POCO 1000UL
#define USECPERSEC_POCO 1000000UL
#define NSECPERSEC_POCO 1000000000UL

#endif
