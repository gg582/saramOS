/* start.S - saramOS x86_64 Bare-metal Entry */

#define MB_MAGIC    0x1BADB002
#define MB_FLAGS    ((1 << 0) | (1 << 1) | (1 << 16))
#define MB_CHECKSUM -(MB_MAGIC + MB_FLAGS)

.section .multiboot
.align 4
multiboot_header:
    .long MB_MAGIC
    .long MB_FLAGS
    .long MB_CHECKSUM
    /* AOUT kludge fields - used if GRUB boots the ELF as a flat binary */
    .long multiboot_header    /* header_addr */
    .long 0x100000            /* load_addr (Fixed at 1MB) */
    .long _edata              /* load_end_addr */
    .long _ebss               /* bss_end_addr */
    .long _start              /* entry_addr */

.section .text
.code32
.global _start
_start:
    cli
    
    /* Initialize stack for 32-bit transition.
       Stack should be 16-byte aligned for the later C call. */
    mov $stack_top, %esp

    /* 0. Zero out paging tables to avoid junk entries */
    mov $p4_table, %edi
    mov $12288, %ecx          /* 4096 * 3 */
    xor %eax, %eax
    rep stosb

    /* 1. Setup Paging (Identity Map 1GB) */
    /* Level 4 Table (PML4) -> PDPT */
    mov $p4_table, %edi
    mov $p3_table, %eax
    or $0x003, %eax           /* Present | Writable */
    mov %eax, (%edi)

    /* Level 3 Table (PDPT) -> PD */
    mov $p3_table, %edi
    mov $p2_table, %eax
    or $0x003, %eax           /* Present | Writable */
    mov %eax, (%edi)

    /* Level 2 Table (PD) - Map 512 entries of 2MB = 1GB identity */
    mov $p2_table, %edi
    xor %ecx, %ecx
.map_p2:
    mov %ecx, %eax
    shl $21, %eax             /* ecx * 2MB */
    or $0x083, %eax           /* Present | Writable | Page Size (2MB Huge Page) */
    mov %eax, (%edi, %ecx, 8)
    movl $0, 4(%edi, %ecx, 8) /* Ensure upper 32-bits are zero */
    inc %ecx
    cmp $512, %ecx
    jne .map_p2

    /* 2. Enable PAE (Physical Address Extension) - Required for Long Mode */
    mov %cr4, %eax
    or $0x20, %eax            /* Bit 5 = PAE */
    mov %eax, %cr4

    /* 3. Load CR3 with PML4 address */
    mov $p4_table, %eax
    mov %eax, %cr3

    /* 4. Enable Long Mode in EFER (Extended Feature Enable Register) */
    mov $0xC0000080, %ecx     /* EFER MSR */
    rdmsr
    or $0x100, %eax           /* LME bit (bit 8) */
    wrmsr

    /* 5. Enable Paging & Protected Mode simultaneously */
    mov %cr0, %eax
    or $0x80000001, %eax      /* Bit 31 = PG, Bit 0 = PE */
    mov %eax, %cr0

    /* 6. Load 64-bit GDT */
    /* Note: lgdt takes a 6-byte pointer in 32-bit mode. */
    lgdt (gdt64_ptr)
    
    /* 7. Far Return to Long Mode */
    /* We push the CS selector (0x08) and the label. lret will pop them. */
    push $0x08                /* Code Segment Selector */
    push $.long_mode          /* Offset (32-bit EIP for now) */
    lret

.code64
.long_mode:
    /* Set segment registers to 0 for 64-bit mode (Flat model) */
    xor %ax, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov %ax, %fs
    mov %ax, %gs

    /* Switch to 64-bit stack pointer */
    mov $stack_top, %rsp

    /* Call Kernel Main. 
       RSP is 16-byte aligned here if stack_top was aligned. */
    call kmain

.hang:
    hlt
    jmp .hang

.section .rodata
.align 8
gdt64:
    .quad 0x0000000000000000  /* Null Descriptor */
    /* 64-bit Code Segment Descriptor:
       Access: 0x9A (Present, Ring 0, Code, Exec/Read)
       Flags:  0x20 (L=1 for Long Mode) -> Note: GDT entry flags are bits 52-55. 
       Full Quad: 0x00AF9A000000FFFF (Standard for 64-bit kernel) */
    .quad 0x00af9a000000ffff
gdt64_end:

.align 8
gdt64_ptr:
    .word gdt64_end - gdt64 - 1 /* Limit */
    .long gdt64                 /* Base (32-bit) */
    .long 0                     /* Upper 32-bit of base (Padding/Zero) */

.section .bss
.align 4096
p4_table: .skip 4096
p3_table: .skip 4096
p2_table: .skip 4096

.align 16
.global stack_bottom
stack_bottom: .skip 16384       /* 16KB Stack */
.global stack_top
stack_top:
