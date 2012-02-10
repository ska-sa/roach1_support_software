/* flushing primitives */
/* Marc Welz <marc@ska.ac.za> */

  .file	"flush.S"
  .text

  .globl flush
flush:
   sync
   blr

  .globl barrier
barrier:
   eieio
   blr
