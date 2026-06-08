    .syntax unified
    .cpu cortex-m7
    .fpu fpv5-sp-d16
    .thumb

    .section .isr_vector,"a",%progbits
    .global g_pfnVectors
    .type g_pfnVectors, %object
g_pfnVectors:
    .word _estack
    .word Reset_Handler
    .word Default_Handler      /* NMI */
    .word HardFault_Handler
    .word Default_Handler      /* MemManage */
    .word Default_Handler      /* BusFault */
    .word Default_Handler      /* UsageFault */
    .word 0
    .word 0
    .word 0
    .word 0
    .word Default_Handler      /* SVC */
    .word Default_Handler      /* DebugMon */
    .word 0
    .word PendSV_Handler
    .word Default_Handler      /* SysTick */
    .size g_pfnVectors, .-g_pfnVectors

    .text
    .global Reset_Handler
    .type Reset_Handler, %function
Reset_Handler:
    /* Copy .data from FLASH to RAM */
    ldr r0, =_sdata
    ldr r1, =_edata
    ldr r2, =_sidata
    b 2f
1:
    ldr r3, [r2], #4
    str r3, [r0], #4
2:
    cmp r0, r1
    blo 1b

    /* Zero .bss */
    ldr r0, =_sbss
    ldr r1, =_ebss
    movs r2, #0
    b 4f
3:
    str r2, [r0], #4
4:
    cmp r0, r1
    blo 3b

    /* Call system init */
    bl hal_system_init

    /* Call main */
    bl main

    /* Loop forever if main returns */
    b .

    .type Default_Handler, %function
Default_Handler:
    b Default_Handler
