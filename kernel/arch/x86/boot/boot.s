/* boot/boot.S — Multiboot v1 header + simple entry with correct serial out */
    .section .multiboot
    .align 4
    .long 0x1BADB002            /* magic */
    .long 0x00000003            /* flags: request memory info and boot device */
    .long -(0x1BADB002 + 0x00000003) /* checksum */

    .section .text
    .code32
    .global start
    .extern kmain

start:
    cli
    xor %ebp, %ebp
    mov $0x90000, %esp

    /* very early serial probe: write "BOOT\n" to COM1 (port 0x3F8) */
    movb $'B', %al
    mov $0x3F8, %dx
    outb %al, %dx

    movb $'O', %al
    outb %al, %dx

    movb $'O', %al
    outb %al, %dx

    movb $'T', %al
    outb %al, %dx

    movb $'\n', %al
    outb %al, %dx

    /* Push multiboot info ptr (GRUB passes it in EBX) and call kernel entry */
    push %ebx
    call kmain

.hang:
    hlt
    jmp .hang