/* A simple utility to translate binary into a format 
 * which can be loaded as a macro file by ocd commander
 *
 * Marc Welz (marc@ska.ac.za)
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sysexits.h>

#include <netinet/in.h>

void usage(char *app)
{
  printf("%s [-p pad] [-b base] [-s] binary-file\n", app);
  printf("-s        skip whitespaces\n");
  printf("-b base   start at the given base address\n");
  printf("-p pad    pad out up to given end address\n");
}

int main(int argc, char **argv)
{
  uint32_t w, base, p;
  unsigned int i, j, pad;
  int skip, c, r;
  char *app;
  FILE *inf;

  p = 0;
  app = argv[0];
  base = 0;
  skip = 0;
  inf = NULL;
  pad = 0;

  i = j = 1;
  while (i < argc) {
    if (argv[i][0] == '-') {
      c = argv[i][j];
      switch (c) {
        case 'h' :
          usage(app);
          return EX_OK;

        case 'b' : 
        case 'p' : 
          j++;
          if (argv[i][j] == '\0') {
            j = 0;
            i++;
          }
          if (i >= argc) {
            fprintf(stderr, "%s: argument needs a parameter\n", app);
            return EX_USAGE;
          }

          switch(c){
            case 'b' :
              base = strtoul(argv[i] + j, NULL, 0);
              break;
            case 'p' :
              pad = strtoul(argv[i] + j, NULL, 0);
              break;
          }

          i++;
          j = 1;
          break;

        case 's' : 
          skip = 1;
          j++;
          break;

          j++;
          break;
        case '\0':
          j = 1;
          i++;
          break;
        default:
          fprintf(stderr, "%s: unknown option -%c\n", app, argv[i][j]);
          return EX_USAGE;
      }
    } else {

      if(inf){
        fprintf(stderr, "%s: duplicate input file name %s\n", app, argv[i]);
        return EX_USAGE;
      }

      inf = fopen(argv[i], "r");
      if(inf == NULL){
        fprintf(stderr, "%s: unable to open %s for reading\n", app, argv[i]);
        return EX_OSERR;
      }

      i++;
    }
  }

  if(inf == NULL){
    inf = stdin;
  }

  while((r = (fread(&w, 4, 1, inf))) == 1){
    if((skip == 0) || (w || p)){
      fprintf(stdout, "word 0x%08x = 0x%08x\n", base, htonl(w));
    }
    p = w;
    base += 4;
  }

  while(base < pad){
    fprintf(stdout, "word 0x%08x = 0x00000000\n", base);
    base += 4;
  }

  return 0;
}
