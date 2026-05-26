.section .entry, "ax"
.global _start
.extern main
.extern esp32_hal_early_init

_start:
    l32r    a1, .Lstack_top        # set stack pointer
    movi    a0, 0

    l32r    a2, .Ldata_load
    l32r    a3, .Ldata_start
    l32r    a4, .Ldata_end
copy_data:
    beq     a3, a4, clear_bss
    l32i    a5, a2, 0
    s32i    a5, a3, 0
    addi    a2, a2, 4
    addi    a3, a3, 4
    j       copy_data

clear_bss:
    l32r    a2, .Lbss_start
    l32r    a3, .Lbss_end
    beq     a2, a3, boot_call
    movi    a4, 0
bss_loop:
    s32i    a4, a2, 0
    addi    a2, a2, 4
    bltu    a2, a3, bss_loop

boot_call:
    call0   esp32_hal_early_init
    call0   main

halt:
    j       halt

.literal .Lstack_top, __stack_top
.literal .Ldata_load, __data_load_start__
.literal .Ldata_start, __data_start__
.literal .Ldata_end, __data_end__
.literal .Lbss_start, __bss_start__
.literal .Lbss_end, __bss_end__
