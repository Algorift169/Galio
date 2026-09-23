/* AP startup trampoline copied to low physical memory before INIT/SIPI. */
.section .ap_trampoline, "ax"
.code16
.global ap_trampoline_start
.global ap_trampoline_end
.global ap_protected_mode
.global ap_long_mode
.extern ap_trampoline_entry

/* Shared startup parameters live at physical 0x9000. */
.equ AP_PARAMS, 0x9000
.equ AP_CR3, AP_PARAMS + 0
.equ AP_STACK, AP_PARAMS + 8
.equ AP_ENTRY, AP_PARAMS + 16
.equ AP_GDT, AP_PARAMS + 24
.equ AP_PROTECTED_JUMP, AP_PARAMS + 30
.equ AP_LONG_JUMP, AP_PARAMS + 36

ap_trampoline_start:
    cli
    cld
    xor %ax, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss

    mov AP_CR3, %eax
    mov %eax, %cr3

    lgdt AP_GDT
    mov %cr0, %eax
    or $0x1, %eax
    mov %eax, %cr0

    ljmp *AP_PROTECTED_JUMP

.code32
ap_protected_mode:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    mov %cr4, %eax
    or $0x20, %eax
    mov %eax, %cr4

    mov $0xC0000080, %ecx
    rdmsr
    or $0x100, %eax
    wrmsr

    mov %cr0, %eax
    or $0x80000000, %eax
    mov %eax, %cr0

    ljmp *AP_LONG_JUMP

.code64
ap_long_mode:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    mov AP_STACK, %rsp
    xor %rbp, %rbp
    mov AP_ENTRY, %rax
    call *%rax

ap_hang:
    cli
    hlt
    jmp ap_hang

ap_trampoline_end:
.section .note.GNU-stack,"",@progbits
