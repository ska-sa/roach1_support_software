#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <katcp.h>

#include "misc.h"

/********************************************************************************/

/********************************************************************************/

static unsigned int digit_to_value(char *string, int len)
{
	unsigned int value;
	int i;

	value = 0;

	for(i = 0; i < len; i++){
		value *= 10;
		value += (string[i] - '0');
	}

	return value;
}

struct segment_index{
	char *ptr;
	int len;
	unsigned int result;
};

int time_from_string(struct timeval *tv, unsigned int *femto, char *str)
{
	struct segment_index lookup[4];
	int i, run, dot, end, pre, result, fixup;
	unsigned int factor;

	run = 1;
	i = 0;
	dot = (-1);     /* seen no dot sofar */
	fixup = (-1);
        end = (-1);

	while(run){
		switch(str[i]){
			case '1' :
			case '2' :
			case '3' :
			case '4' :
			case '5' :
			case '6' :
			case '7' :
			case '8' :
			case '9' :
			case '0' :
				i++;
				break;
			case '.' :
				dot = i;
				i++;
				break;
			default :
				run = 0;
				end = i;
		}
	}

#ifdef DEBUG
	fprintf(stderr, "time to string: string <%s> has dot at %d, end after %d\n", str, dot, end);
#endif

	if(end <= 0){
		return -1;
	}

	for(i = 0; i < 4; i++){
		lookup[i].ptr = NULL;
		lookup[i].len = 0;
		lookup[i].result = 0;
	}

	if(dot < 0){
		pre = end;
	} else {
		pre = dot;
	}

	if(pre > 3){
		lookup[0].ptr = str;
		lookup[0].len = pre - 3;
		lookup[1].ptr = str + pre - 3;
		lookup[1].len = 3;
	} else if(pre > 0){
		lookup[1].ptr = str;
		lookup[1].len = pre;
	}

	factor = 1;
	fixup = (-1);

	if(dot >= 0){
		if(end > (dot + 4)){
#ifdef DEBUG
			fprintf(stderr, "time to string: have a femto component\n");
#endif
			lookup[2].ptr = str + dot + 1;
			lookup[2].len = 3;

			lookup[3].ptr = str + dot + 4;
			lookup[3].len = end - (dot + 4);
			if(lookup[3].len > 9){
				lookup[3].len = 9;
			} else {
				fixup = 3;
				for(i = lookup[fixup].len; i < 9; i++){
					factor *= 10;
				}
			}
		} else if(end > (dot + 1)){
			lookup[2].ptr = str + dot + 1;
			lookup[2].len = end - (dot + 1);
			fixup = 2;
			for(i = lookup[fixup].len; i < 3; i++){
				factor *= 10;
			}
		}
	}

	result = 0;

	for(i = 0; i < 4; i++){
		lookup[i].result = digit_to_value(lookup[i].ptr, lookup[i].len);
#ifdef DEBUG
		fprintf(stderr, "time to string[%d]: conversion of %s(%d) -> %u\n", i, lookup[i].ptr, lookup[i].len, lookup[i].result);
#endif
	}
	if(fixup >= 0){
		lookup[fixup].result *= factor;
#ifdef DEBUG
		fprintf(stderr, "time to string[%d]: fixup -> %u\n", fixup, lookup[fixup].result);
#endif
	}

	if(tv){
		tv->tv_sec = lookup[0].result;
		tv->tv_usec = (lookup[1].result * 1000) + lookup[2].result;
#ifdef DEBUG
		fprintf(stderr, "time to string: str %s -> tv <%lu.%06lu>\n", str, tv->tv_sec, tv->tv_usec);
#endif
	} else {
		for(i = 0; i < 3; i++){
			if(lookup[i].result){
				result = 1;
			}
		}
	}

	if(femto){
		*femto = lookup[3].result;
#ifdef DEBUG
		fprintf(stderr, "time to string: str %s -> femto component %u\n", str, *femto);
#endif
	} else {
		if(lookup[3].result){
			result = 1;
		}
	}

	return result;
}

/********************************************************************************/

int shift_point_string(long *value, char *buffer, unsigned int shift)
{
	unsigned int prefix, i, suffix, limit, nonzero;
	int sign, dot;
	long tmp;

	limit = (shift > 9) ? 0 : (9 - shift);

	suffix = 0;
	prefix = 0;
	nonzero = 0;

	dot = (-1);
	tmp = 0;

	i = 0;
	sign = 1;
	if(buffer[i] == '-'){
		sign = (-1);
		i++;
	}

	while(1){
		switch(buffer[i]){
			case '1' :
			case '2' :
			case '3' :
			case '4' :
			case '5' :
			case '6' :
			case '7' :
			case '8' :
			case '9' :
				nonzero = 1;
				/* FALL */
			case '0' :
				if(dot >= 0){
					suffix++;
					if(suffix > shift){
						fprintf(stderr, "shift: got more than %d digits past decimal\n", shift);
						*value = sign * tmp;
						return 1;
					}
				} else {
					prefix += nonzero;
					if(prefix > limit){
						fprintf(stderr, "shift: number too large, more than %d digits before the decimal\n", limit);
						return 1;
					}
				}
				tmp *= 10;
				tmp += (buffer[i] - '0');
				break;
			case '.' :
#ifdef DEBUG
				fprintf(stderr, "shift: found dot at %d, value sofar is %ld\n", i, tmp);
#endif
				if(dot >= 0){
					fprintf(stderr, "shift: saw more than one dot, at %d and %d\n", dot, i);
					return -1;
				}
				dot = i;
				nonzero = 0;
				break;
			case '\0' :
			case '\r' :
			case '\n' :
			case '\t' :
			case ' '  :
				while(suffix < shift){
					tmp *= 10;
					suffix++;
				}
				*value = sign * tmp;
				return 0;
			default :
				return -1;
		}
		i++;
	}
}
