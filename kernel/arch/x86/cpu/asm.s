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
    ; register_state_t offsets:
    ; rsp=0x00, rbp=0x08, rbx=0x20, rflags=0x40, rip=0x48.
    test rdi, rdi
    jz .load_new

    ; Remove this call's return address from the live stack and save it as
    ; the resume address. The saved rsp must point above that address.
    pop rax
    mov [rdi + 0x00], rsp
    mov [rdi + 0x08], rbp
    pushfq
    pop qword [rdi + 0x40]
    mov [rdi + 0x20], rbx
    mov [rdi + 0x48], rax

.load_new:
    test rsi, rsi
    jz .done

    mov rsp, [rsi + 0x00]
    mov rbp, [rsi + 0x08]
    mov rbx, [rsi + 0x20]
    push qword [rsi + 0x40]
    popfq
    mov rax, [rsi + 0x48]
    jmp rax

.done:
    ret