/* Primitive IO library for rinit */
/* Marc Welz <marc@ska.ac.za> */

#include "serial.h"
#include "lib.h"

static unsigned int nibble_to_hex(unsigned int n)
{
  unsigned int t;

  t = n & 0xf;

  return (t < 10) ? (t + 0x30) : (t + 0x57);
}

void print_hex_unsigned(unsigned int x)
{
  int i;

  for(i = 0; i < 8 ; i++){
    print_char(nibble_to_hex(x >> ((7 - i) * 4)));
  }
}

void print_newline()
{
  print_string("\r\n");
}

void print_string(char *s)
{
  while(*s != '\0'){
    print_char(*s);
    s++;
  }
}

unsigned int get_hex_unsigned()
{
  unsigned int v, c;

  v = 0;
  for(;;){
    c = test_char();
    switch(c){
      case SERIAL_TIMEOUT :
        continue;
      case '0' :
      case '1' :
      case '2' :
      case '3' :
      case '4' :
      case '5' :
      case '6' :
      case '7' :
      case '8' :
      case '9' :
        v = (v << 4) | ((c - '0') & 0xf);
        break;
      case 'a' :
      case 'b' :
      case 'c' :
      case 'd' :
      case 'e' :
      case 'f' :
        v = (v << 4) | ((c - 'W') & 0xf);
        break;
      default :
        print_string("\r\n");
        return v;
    }
    print_char(c);
  }
}

unsigned int get_byte(unsigned int *timeout)
{
  unsigned int result;

  result = test_char();
  for(; *timeout > 0; *timeout = (*timeout) - 1){
    if(result != SERIAL_TIMEOUT){
      return result;
    }
    result = test_char();
  }
  
  return result;
}
