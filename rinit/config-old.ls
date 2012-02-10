_STACK_SIZE = DEFINED(_STACK_SIZE) ? _STACK_SIZE : 0x200;
_HEAP_SIZE = DEFINED(_HEAP_SIZE) ? _HEAP_SIZE : 0x40;
_START_RAM = 0xfffff000;

MEMORY
{
  blockram : ORIGIN = 0xfffff000, LENGTH = 0x1000 - 4
  boot     : ORIGIN = 0xfffffffc, LENGTH = 4
}

PHDRS
{
  program PT_LOAD ;
  data1 PT_LOAD ;
  data2 PT_LOAD ;
  stack PT_LOAD ;
  heap PT_LOAD ;
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

  .bss_stack    :
  {
    /* add stack and align to 16 byte boundary */
    . = . + _STACK_SIZE;
    . = ALIGN(16);
    /* stack grows down */
    __stack = .;
  } > blockram : stack

  .bss_heap    :
  {
    /* add heap and align to 16 byte boundary */
     . = ALIGN(16);
    PROVIDE(_heap_start = .);
    . = . + _HEAP_SIZE;
    . = ALIGN(16);
    PROVIDE(_heap_end = .);
  } > blockram : heap

  /* do we really need all the end symbols ? */
  _end = . ;
  PROVIDE (end = .);
  PROVIDE (__end = .);

  PROVIDE (_serial_xmit = 0xd0ffdf04);

  /* processor starts here */
  .boot 0xFFFFFFFC : { *(.boot) } : boot
}
