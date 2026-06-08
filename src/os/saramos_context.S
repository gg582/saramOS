    .syntax unified
    .cpu cortex-m7
    .fpu fpv5-sp-d16
    .thumb

    .text

    .global PendSV_Handler
    .type PendSV_Handler, %function
PendSV_Handler:
    mrs r0, psp
    ldr r1, =saramos_current_tcb
    ldr r2, [r1]
    cbz r2, 1f
    stmdb r0!, {r4-r11}
    str r0, [r2]

1:
    ldr r3, =saramos_next_tcb
    ldr r2, [r3]
    cbz r2, 2f
    str r2, [r1]
    ldr r0, [r2]
    ldmia r0!, {r4-r11}
    msr psp, r0

2:
    bx lr

    .global HardFault_Handler
    .type HardFault_Handler, %function
HardFault_Handler:
    tst lr, #4
    ite eq
    mrseq r0, msp
    mrsne r0, psp
    mov r1, lr
    b saramos_hardfault_dispatch

    .global saramos_start_tcb
    .type saramos_start_tcb, %function
saramos_start_tcb:
    ldr r0, [r0]
    ldmia r0!, {r4-r11}
    msr psp, r0
    movs r0, #2
    msr control, r0
    isb
    pop {r0-r3, r12, lr}
    pop {r1}
    pop {r2}
    bx r1

    .global saramos_fault_switch_to
    .type saramos_fault_switch_to, %function
saramos_fault_switch_to:
    ldr r0, [r0]
    ldmia r0!, {r4-r11}
    msr psp, r0
    movw lr, #0xfffd
    movt lr, #0xffff
    bx lr
