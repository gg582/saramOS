/* Minimal ESP32-WROOM32 linker script for bare-metal hello_rtos */
ENTRY(_start)

MEMORY
{
  iram (rx)  : ORIGIN = 0x40080000, LENGTH = 0x00040000
  dram (rw)  : ORIGIN = 0x3FFB0000, LENGTH = 0x00050000
}

__stack_top = ORIGIN(dram) + LENGTH(dram);

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

  .data ALIGN(16) : AT(LOADADDR(.text) + SIZEOF(.text))
  {
    __data_load_start = LOADADDR(.data);
    __data_start = .;
    *(.data .data.*)
    *(.sdata .sdata.*)
    __data_end = .;
  } > dram

  .bss (NOLOAD) ALIGN(16) :
  {
    __bss_start = .;
    *(.sbss .sbss.*)
    *(.bss .bss.*)
    *(COMMON)
    __bss_end = .;
  } > dram

  .stack (NOLOAD) ALIGN(16) :
  {
    PROVIDE(__stack_start = .);
    . = __stack_top;
  } > dram
}

PROVIDE(_stack_top = __stack_top);
PROVIDE(__data_start__ = __data_start);
PROVIDE(__data_end__ = __data_end);
PROVIDE(__data_load_start__ = __data_load_start);
PROVIDE(__bss_start__ = __bss_start);
PROVIDE(__bss_end__ = __bss_end);
