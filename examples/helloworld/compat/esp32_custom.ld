/* Minimal ESP32-WROOM32 linker script for bare-metal hello_rtos */
ENTRY(_start)

MEMORY
{
  iram (rx)  : ORIGIN = 0x40080000, LENGTH = 0x00040000
  dram (rw)  : ORIGIN = 0x3FFB0000, LENGTH = 0x00050000
}

SECTIONS
{
  .vectors :
  {
    KEEP(*(.vectors))
    KEEP(*(.entry))
    KEEP(*(.entry.text))
  } > iram

  .text :
  {
    _text_start = .;
    *(.literal .literal.*)
    *(.text.startup .text.startup.*)
    *(.text .text.*)
    *(.rodata .rodata.*)
    *(.init .init.*)
    *(.fini .fini.*)
    _text_end = .;
  } > iram

  .data :
  {
    __data_load_start__ = LOADADDR(.data);
    __data_start__ = .;
    *(.data .data.*)
    *(.sdata .sdata.*)
    __data_end__ = .;
  } > dram

  .bss (NOLOAD) :
  {
    __bss_start__ = .;
    *(.sbss .sbss.*)
    *(.bss .bss.*)
    *(COMMON)
    __bss_end__ = .;
  } > dram

  .stack (NOLOAD) :
  {
    . = ALIGN(16);
    __stack_start__ = .;
    . += 32768;
    __stack_top = .;
  } > dram

  /* End of used DRAM, start of heap */
  _end = .;
  PROVIDE(end = .);
  PROVIDE(__end = .);
}

PROVIDE(_stack_top = __stack_top);
PROVIDE(__stack_top = __stack_top);
