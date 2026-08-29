; src/asm.S
; Provides gdt_flush, idt_load, and paging operations
BITS 64
GLOBAL gdt_flush
GLOBAL idt_load
GLOBAL paging_enable_asm

; gdt_flush: arg = pointer to gdt_ptr (limit:16, base:64)
gdt_flush:
    lgdt [rdi]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    push qword 0x08
    lea rax, [rel .reload_cs]
    push rax
    retfq
.reload_cs:
    ret

; idt_load: arg = pointer to idt_ptr (limit:16, base:64)
idt_load:
    lidt [rdi]
    ret

; paging_enable_asm: arg = page directory address
paging_enable_asm:
    mov rax, rdi
    mov cr3, rax
    mov rax, cr0
    or rax, 0x80000000
    mov cr0, rax
    ret

extern process_switch_new_eflags
extern process_switch_new_eip

GLOBAL process_switch_asm
process_switch_asm:
    test rdi, rdi
    jz .load_new

    mov [rdi + 0x00], rsp
    mov [rdi + 0x08], rbp
    mov [rdi + 0x20], rbx
    mov [rdi + 0x68], r12
    mov [rdi + 0x70], r13
    mov [rdi + 0x78], r14
    mov [rdi + 0x80], r15
    pop rax
    mov [rdi + 0x48], rax
    push rax

.load_new:
    test rsi, rsi
    jz .done

    mov rsp, [rsi + 0x00]
    mov rbp, [rsi + 0x08]
    mov rbx, [rsi + 0x20]
    mov r12, [rsi + 0x68]
    mov r13, [rsi + 0x70]
    mov r14, [rsi + 0x78]
    mov r15, [rsi + 0x80]
    mov rax, [rsi + 0x48]
    jmp rax

.done:
    ret