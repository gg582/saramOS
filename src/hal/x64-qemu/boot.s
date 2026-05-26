; MultiBoot Header
MB_MAGIC equ 0x1BADB002
MB_FLAGS equ 1 | 2
MB_CHECKSUM equ -(MB_MAGIC + MB_FLAGS)

section .multiboot
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM

section .bootstrap_stack, "aw", @nobits
stack_bottom:
    resb 16384 ; 16 KiB
stack_top:

section .text
global _start
extern kmain

_start:
    mov esp, stack_top
    call kmain

    cli
.hang:
    hlt
    jmp .hang
