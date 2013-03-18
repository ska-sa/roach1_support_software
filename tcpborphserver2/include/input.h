#ifndef INPUT_POCO_H_
#define INPUT_POCO_H_

#include <katcp.h>

#define ANT_EXTRACT_POCO 0
#define POLARISATION_EXTRACT_POCO 1
#define CHANNEL_EXTRACT_POCO 2

int extract_field_poco(char *string, int what);

int print_field_poco(char *string, int len, int index, int channel);

#define extract_input_poco extract_ant_poco

int extract_ant_poco(char *string);
int extract_polarisation_poco(char *string);
int extract_channel_poco(char *string);

#endif
