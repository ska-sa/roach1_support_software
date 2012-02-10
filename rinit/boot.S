/* First instruction of rinit, jumps back 4k from end of flash */
/* Marc Welz <marc@ska.ac.za> */

	.file "boot.S"

	.section .boot,"ax"
	.global _boot

_boot:	
	b _start
