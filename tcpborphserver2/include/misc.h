#ifndef MISC_POCO_H_
#define MISC_POCO_H_

#include <katcp.h>

/* misc support */
int time_from_string(struct timeval *tv, unsigned int *femto, char *str);
int shift_point_string(long *value, char *buffer, unsigned int shift);

#define MSECPERSEC_POCO 1000UL
#define USECPERSEC_POCO 1000000UL
#define NSECPERSEC_POCO 1000000000UL

#endif
