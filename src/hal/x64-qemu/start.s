; Multiboot Header
MB_MAGIC    equ 0x1BADB002
MB_FLAGS    equ 0x00000003
MB_CHECKSUM equ -(MB_MAGIC + MB_FLAGS)

section .multiboot
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM

section .text
[BITS 32]
global _start
extern kmain

_start:
    cli
    mov esp, stack_top

    ; Debug: Print 'A'
    mov dx, 0x3f8
    mov al, 'A'
    out dx, al

    ; 0. Zero out paging tables
    mov edi, p4_table
    mov ecx, 12288
    xor eax, eax
    rep stosb

    ; 1. Setup Paging (Identity Map first 1GB)
    mov eax, p3_table
    or eax, 0b11
    mov [p4_table], eax

    mov eax, p2_table
    or eax, 0b11
    mov [p3_table], eax

    mov ecx, 0
.map_p2_table:
    mov eax, ecx
    shl eax, 21
    or eax, 0b10000011
    mov [p2_table + ecx * 8], eax
    mov dword [p2_table + ecx * 8 + 4], 0

    inc ecx
    cmp ecx, 512
    jne .map_p2_table

    ; 2. Enable PAE
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; 3. Load P4 to CR3
    mov eax, p4_table
    mov cr3, eax

    ; 4. Enable Long Mode
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; 5. Enable Paging
    mov eax, cr0
    or eax, (1 << 31) | (1 << 0)
    mov eax, eax
    mov cr0, eax

    ; 6. Jump to 64-bit
    lgdt [gdt64.pointer]
    jmp 0x08:.long_mode

[BITS 64]
.long_mode:
    mov ax, 0
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov rsp, stack_top
    call kmain

    cli
.hang:
    hlt
    jmp .hang

section .rodata
align 8
gdt64:
    dq 0 ; Null
    dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53) ; Code
.pointer:
    dw $ - gdt64 - 1
    dq gdt64

section .bss
align 4096
p4_table:
    resb 4096
p3_table:
    resb 4096
p2_table:
    resb 4096
stack_bottom:
    resb 16384
stack_top:
