; src/asm.S
; Provides gdt_flush, idt_load, and paging operations
BITS 32
GLOBAL gdt_flush
GLOBAL idt_load
GLOBAL paging_enable_asm

; gdt_flush: arg = pointer to gdt_ptr (limit:16, base:32)
gdt_flush:
    mov eax, [esp + 4]      ; address of gdt_ptr
    lgdt [eax]
    ; reload segments
    mov ax, 0x10            ; data selector (index 2)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ; far jump to reload cs
    jmp 0x08:flush_cs
flush_cs:
    ret

; idt_load: arg = pointer to idt_ptr (limit:16, base:32)
idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret

; paging_enable_asm: arg = page directory address
paging_enable_asm:
    mov eax, [esp + 4]
    mov cr3, eax            ; Load page directory
    mov eax, cr0
    or eax, 0x80000000     ; Set PG bit
    mov cr0, eax            ; Enable paging
    ret

extern process_switch_new_eflags
extern process_switch_new_eip

; process_switch_asm: arg1 = old_regs, arg2 = new_regs
GLOBAL process_switch_asm
process_switch_asm:
    ; Save register pointers
    mov edi, [esp + 4]      ; old_regs
    mov ebx, [esp + 8]      ; new_regs

    ; Save current registers into old_regs
    ; After pushad, stack contains: [esp+0]=eax, [esp+4]=ecx, [esp+8]=edx, [esp+12]=ebx,
    ; [esp+16]=orig_esp, [esp+20]=ebp, [esp+24]=esi, [esp+28]=edi
    pushad
    mov eax, esp
    mov ecx, [eax + 16]
    mov [edi + 0], ecx          ; esp (original ESP is at offset 16)
    mov ecx, [eax + 20]
    mov [edi + 4], ecx          ; ebp (EBP is at offset 20)
    mov ecx, [eax + 24]
    mov [edi + 8], ecx          ; esi (ESI is at offset 24)
    mov ecx, [eax + 28]
    mov [edi + 12], ecx         ; edi (EDI is at offset 28)
    mov ecx, [eax + 12]
    mov [edi + 16], ecx         ; ebx (EBX is at offset 12)
    mov ecx, [eax + 8]
    mov [edi + 20], ecx         ; edx (EDX is at offset 8)
    mov ecx, [eax + 4]
    mov [edi + 24], ecx         ; ecx (ECX is at offset 4)
    mov ecx, [eax + 0]
    mov [edi + 28], ecx         ; eax (EAX is at offset 0)
    add esp, 32
    pushfd
    pop dword [edi + 32]        ; eflags

    ; Save new_regs eflags/eip temporarily before overriding ebx
    mov edx, [ebx + 32]
    mov [process_switch_new_eflags], edx
    mov edx, [ebx + 36]
    mov [process_switch_new_eip], edx

    ; Load registers from new_regs and switch stacks
    mov esi, ebx                ; new_regs pointer
    mov eax, [esi + 28]
    mov ecx, [esi + 24]
    mov edx, [esi + 20]
    mov ebx, [esi + 16]
    mov edi, [esi + 12]
    mov ebp, [esi + 4]
    mov esp, [esi + 0]
    ; Load EFLAGS for new process
    push dword [process_switch_new_eflags]
    popfd

    ; Decide transition method based on CPL required by CS selector
    mov ax, word [esi + 40]     ; new CS selector (16-bit)
    test ax, 0x3                ; check RPL bits
    jz .kernel_mode_switch

    ; User-mode target: build an IRET frame (SS, ESP, EFLAGS, CS, EIP)
    ; Push user SS (word) then user ESP (dword)
    mov bx, word [esi + 48]     ; user_ss (use esi which still points to new_regs)
    push bx                     ; pushw user_ss
    mov eax, [esi + 44]         ; user_esp (use esi, not the corrupted edx)
    push dword eax              ; pushl user_esp
    ; Push EFLAGS (already loaded into EFLAGS, but push saved value)
    push dword [process_switch_new_eflags]
    ; Push CS and EIP
    mov ax, word [esi + 40]     ; Reload CS selector to ax (was lost)
    push word ax                ; pushw user CS
    push dword [process_switch_new_eip]
    iret

.kernel_mode_switch:
    ; Kernel-mode: return normally (CS unchanged)
    push dword [process_switch_new_eip]
    ret