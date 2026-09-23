; Galio Kernel
; Copyright (C) 2026 S.M Israfil
; This file is part of Galio.

; isr_asm.s - ISR and IRQ stubs with proper argument passing

BITS 64

%define USER64_DS 0x1B
%define USER64_CS 0x23

extern isr_handler
extern irq_handler
extern syscall_handler
extern syscall_fast_scratch
extern tss_entry

%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push 0
    push %1
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push %1
    jmp isr_common_stub
%endmacro

%macro IRQ_STUB 2
global irq%1
irq%1:
    push 0
    push %2
    jmp irq_common_stub
%endmacro

ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE 8
ISR_NOERRCODE 9
ISR_ERRCODE 10
ISR_ERRCODE 11
ISR_ERRCODE 12
ISR_ERRCODE 13
ISR_ERRCODE 14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE 17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_ERRCODE 30
ISR_NOERRCODE 31

IRQ_STUB 0, 32
IRQ_STUB 1, 33
IRQ_STUB 2, 34
IRQ_STUB 3, 35
IRQ_STUB 4, 36
IRQ_STUB 5, 37
IRQ_STUB 6, 38
IRQ_STUB 7, 39
IRQ_STUB 8, 40
IRQ_STUB 9, 41
IRQ_STUB 10, 42
IRQ_STUB 11, 43
IRQ_STUB 12, 44
IRQ_STUB 13, 45
IRQ_STUB 14, 46
IRQ_STUB 15, 47
IRQ_STUB 16, 48
IRQ_STUB 17, 49
IRQ_STUB 18, 50
IRQ_STUB 19, 51
IRQ_STUB 20, 52
IRQ_STUB 21, 53
IRQ_STUB 22, 54
IRQ_STUB 23, 55
IRQ_STUB 24, 56
IRQ_STUB 25, 57
IRQ_STUB 26, 58
IRQ_STUB 27, 59
IRQ_STUB 28, 60
IRQ_STUB 29, 61
IRQ_STUB 30, 62
IRQ_STUB 31, 63

global isr_syscall
isr_syscall:
    push 0
    push 0x80
    jmp isr_common_stub

; Native x86_64 syscall entry. SYSCALL does not save RSP and only preserves
; the return RIP/RFLAGS in RCX/R11, so save those values before switching to
; the current TSS kernel stack. The first 120 bytes below the dispatcher frame
; preserve the user's registers; the dispatcher frame itself is registers_t.
global syscall_fast_entry
syscall_fast_entry:
    mov [rel syscall_fast_scratch + 0], rsp
    mov [rel syscall_fast_scratch + 8], rcx
    mov [rel syscall_fast_scratch + 16], r11
    mov rsp, [rel tss_entry + 4]
    and rsp, -16

    mov ax, 0x10
    mov ds, ax
    mov es, ax

    ; Preserve every user register before reusing the dispatcher frame for
    ; the legacy INT 0x80 argument layout.
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    push qword USER64_DS
    push qword [rel syscall_fast_scratch + 0]
    push qword [rel syscall_fast_scratch + 16]
    push qword [rel syscall_fast_scratch + 8]
    push qword USER64_CS
    push qword 0
    push qword 0x81
    push rax
    push rdi
    push rsi
    push rdx
    push r10
    push r8
    push r9
    push r9
    push r10
    push qword [rel syscall_fast_scratch + 16]
    push r12
    push r13
    push r14
    push r15
    mov rdi, rsp
    call syscall_handler

    ; Return value is owned by the dispatcher frame. Restore the user's
    ; preserved registers, then use the saved RIP/RFLAGS/RSP for SYSRETQ.
    mov rax, [rsp + 112]
    mov rdx, [rsp + 176 + 88]
    mov rsi, [rsp + 176 + 80]
    mov rdi, [rsp + 176 + 72]
    mov rbp, [rsp + 176 + 64]
    mov r8,  [rsp + 176 + 56]
    mov r9,  [rsp + 176 + 48]
    mov r10, [rsp + 176 + 40]
    mov r12, [rsp + 176 + 24]
    mov r13, [rsp + 176 + 16]
    mov r14, [rsp + 176 + 8]
    mov r15, [rsp + 176]
    mov rcx, [rsp + 136]
    mov r11, [rsp + 152]
    mov rax, [rsp + 112]

    mov rdx, [rsp + 160]
    mov bx, USER64_DS
    mov ds, bx
    mov es, bx
    mov rbx, [rsp + 176 + 104]
    mov rsp, rdx
    db 0x48, 0x0f, 0x07

isr_common_stub:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov rdi, rsp
    call isr_handler

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    add rsp, 16
    iretq

irq_common_stub:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov rdi, rsp
    call irq_handler

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    add rsp, 16
    iretq