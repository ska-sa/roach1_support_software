/* Interrupt vector for rinit */
/* Marc Welz <marc@ska.ac.za> */

#include "ppcstuff.h"
#include "serial.h"

  .file	"intt.S"
  .section .intt,"ax"
  .globl _int_table

  /* WARNING: start of table needs to be 16 aligned */
  /* print out a single character indicating the failure, then stop */
_int_table:
  li      r9,0x63 /* c ritical */
  b       _output_int
  nop
  nop
  li      r9,0x6d /* m achine */
  b       _output_int
  nop
  nop
  li      r9,0x64 /* d ata storage */
  b       _output_int
  nop
  nop
  li      r9,0x69 /* i nstruction storage */
  b       _output_int
  nop
  nop
  li      r9,0x65 /* e xternal input */
  b       _output_int
  nop
  nop
  li      r9,0x61 /* a lignment */
  b       _output_int
  nop
  nop
  li      r9,0x70 /* p rogram */
  b       _output_int
  nop
  nop
  li      r9,0x66 /* f pu unavailable */
  b       _output_int
  nop
  nop
  li      r9,0x73 /* s ystem call */
  b       _output_int
  nop
  nop
  li      r9,0x75 /* a u xilliary processor unavailable */
  b       _output_int
  nop
  nop
  li      r9,0x72 /* dec r ementer */
  b       _output_int
  nop
  nop
  li      r9,0x78 /* fi x ed interval timer */
  b       _output_int
  nop
  nop
  li      r9,0x77 /* w atchdog timer */
  b       _output_int
  nop
  nop
  li      r9,0x74 /* data t lb error */
  b       _output_int
  nop
  nop
  li      r9,0x62 /* instruction tl b  error */
  b       _output_int
  nop
  nop
  li      r9,0x67 /* debu g */
  b       _output_int
  nop
  nop

  /* display condition */
_output_int:
  lis     r10,(SERIAL_OUT)@h
  ori     r10,r10,(SERIAL_OUT)@l
  stb     r9,0(r10)
_int_end:
  /* TODO: could attempt a return from int */
  b       _int_end
