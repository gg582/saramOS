/* linker.ld - saramOS x86_64 Kernel Linker Script */
ENTRY(_start)

SECTIONS {
    /* The kernel will be loaded at 1MB by the bootloader */
    . = 1M;
    _kernel_start = .;

    /* Multiboot Header MUST be at the very beginning of the file (within first 8KB) */
    .multiboot : {
        KEEP(*(.multiboot))
    }

    /* Executable code, aligned to 4KB page boundary */
    .text : ALIGN(4K) {
        *(.text)
    }

    /* Read-only data (constants, strings) */
    .rodata : ALIGN(4K) {
        *(.rodata)
    }

    /* Initialized data */
    .data : ALIGN(4K) {
        *(.data)
    }
    _edata = .;

    /* Uninitialized data and stack */
    .bss : ALIGN(4K) {
        *(COMMON)
        *(.bss)
    }
    _ebss = .;

    _kernel_end = .;
}
