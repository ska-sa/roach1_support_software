/* Serial IO functions for rinit */
/* Marc Welz <marc@ska.ac.za> */

#include "ppcstuff.h"
#include "serial.h"

  .file	"serial.S"
  .text

  .globl print_char
print_char:

  /* load base address of serial port */
  lis     r6,(SERIAL_OUT)@h
  ori     r6,r6,(SERIAL_OUT)@l

_no_space:

  /* Here we need to check that space is available, ie fifo not full */
  /* if full, we block, eg jump back to the _no_space label */
  /* use r5, maybe r4 for getting fifo/serial port status */
  /* */
  /* beq */

  lbz     r4,SERIAL_LSR(r6)
  rlwinm. r4,r4,0,26,26
  ble     _no_space

  stb     r3,SERIAL_THR(r6)

  blr


  .globl test_char
test_char:
  
  lis     r6,(SERIAL_OUT)@h
  ori     r6,r6,(SERIAL_OUT)@l

  lbz     r4,SERIAL_LSR(r6)
  rlwinm. r4,r4,0,31,31

  ble     _not_found

  lbz     r3,SERIAL_RBR(r6)
  blr

_not_found:

  li      r3,SERIAL_TIMEOUT
  blr
  

