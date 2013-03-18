#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <katcp.h>

#include "input.h"
#include "config.h"

static char polarisation_name_table[] = "xy";

int print_field_poco(char *string, int len, int index, int channel)
{
  unsigned int ant, pol;

  ant = index / POCO_POLARISATION_COUNT;
  pol = index % POCO_POLARISATION_COUNT;

  return snprintf(string, len, "%d%c", ant, polarisation_name_table[pol]);
}

int extract_field_poco(char *string, int what)
{
  char *end, *tmp;
  int ant, pol, chan;

  ant = (-1);
  pol = (-1);
  chan = (-1);

  ant = strtol(string, &end, 10);

  tmp = NULL;

  switch(end[0]){
    case 'x'  :
    case 'X'  :
      tmp = end + 1;
      pol = 0;
      break;
    case 'y'  :
    case 'Y'  :
      pol = 1;
      tmp = end + 1;
      break;
    case '\0' :
      break;
    default :
      ant = (-1);
      break;
  }
#if 0
  if(tmp){
    chan = strtol(tmp, &end, 10);
    if(end[0] != '\0'){
      chan = (-1);
    }
  }
#endif
  if(tmp){
    if(tmp[0] == '\0'){
      chan = (-1);
    }
    else{
      chan = strtol(tmp, &end, 10);
    }
  }

  switch(what){
    case ANT_EXTRACT_POCO : return ant;
    case POLARISATION_EXTRACT_POCO : return pol;
    case CHANNEL_EXTRACT_POCO : return chan;
    default : return -1;
  }

}

int extract_ant_poco(char *string)
{
  return extract_field_poco(string, ANT_EXTRACT_POCO);
}

int extract_polarisation_poco(char *string)
{
  return extract_field_poco(string, POLARISATION_EXTRACT_POCO);
}

int extract_channel_poco(char *string)
{
  return extract_field_poco(string, CHANNEL_EXTRACT_POCO);
}

