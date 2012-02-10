_START_RAM  = 0x71000000;
_START_INTT = 0x71000ee0;
_START_BOOT = 0x71000ffc;

MEMORY
{
  blockram : ORIGIN = 0x71000000, LENGTH = 0x1000 - 0x48
  intt     : ORIGIN = 0x71000ee0, LENGTH = 0x120 - 4
  boot     : ORIGIN = 0x71000ffc, LENGTH = 4
}

PHDRS
{
  program PT_LOAD ;
  data1 PT_LOAD ;
  data2 PT_LOAD ;
  intt PT_LOAD ;
  boot PT_LOAD ;
}

ENTRY(_boot)
STARTUP(boot.o)

SECTIONS
{
  /* populate blockram with text, static, etc */


  /* .text : */
  .text _START_RAM :
  {
    *(.text)
    *(.text.*)
    *(.gnu.linkonce.t*)
    _etext = .;
    PROVIDE (etext = .);
    PROVIDE (__etext = .);
  } > blockram : program

  .rodata :
  {
    *(.rodata)
    *(.rodata.*)
    *(.gnu.linkonce.r*)
  } > blockram : data1

  .data    :
  {
    *(.data)
    *(.data.*)
    *(.gnu.linkonce.d*)
  } > blockram : data2

  .got1		: 
  { *(.got1) 
  } > blockram 

  .got2		: 
  { *(.got2)
  } > blockram

  .sdata : 
  { 
    *(.sdata) 
  } > blockram

  .sdata2 : 
  { 
    *(.sdata2) 
  } > blockram

  .sbss :
  {
    __sbss_start = .;
    *(.sbss)
    *(.scommon)
    __sbss_end = .;
  } > blockram 

  .bss :
  {
    __bss_start = .;
    *(.bss)
    *(COMMON)
    . = ALIGN(4);
    __bss_end = .;
  } > blockram 

  /* do we really need all the end symbols ? */
  _end = . ;
  PROVIDE (end = .);
  PROVIDE (__end = .);

  /* int vector/handler */
  .intt _START_INTT : { *(.intt) } : intt
  /* processor starts here */
  .boot _START_BOOT : { *(.boot) } : boot
}
