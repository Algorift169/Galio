/*
 * Galio Kernel
 *
 * Copyright (C) 2026 Israfil [Your Legal Name]
 *
 * This file is part of Galio.
 *
 * Galio is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * Galio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Galio. If not, see <https://www.gnu.org/licenses/>.
 */

/* boot.S — Multiboot v1 header + long-mode startup */
.section .multiboot, "a"
.align 4
.long 0x1BADB002
.long 0x00000003
.long -(0x1BADB002 + 0x00000003)

.section .bss
.align 4096
pml4:
    .skip 4096
pdpt:
    .skip 4096
pd0:
    .skip 16384
stack_bottom:
    .skip 65536
stack_top:

.section .text
.code32
.global start
.extern kmain

start:
    cli
    cld
    mov $stack_top, %esp
    mov %ebx, %esi      /* Save Multiboot info pointer in %esi */

    /* Map PML4[0] -> pdpt */
    mov $pml4, %eax
    mov $pdpt, %ebx
    or $0x3, %ebx
    mov %ebx, (%eax)

    /* Map PDPT[0..3] -> pd0, pd1, pd2, pd3 (4 contiguous 4KB PD tables) */
    mov $pdpt, %eax
    mov $pd0, %ebx
    or $0x3, %ebx
    mov %ebx, 0(%eax)
    movl $0, 4(%eax)

    add $4096, %ebx
    mov %ebx, 8(%eax)
    movl $0, 12(%eax)

    add $4096, %ebx
    mov %ebx, 16(%eax)
    movl $0, 20(%eax)

    add $4096, %ebx
    mov %ebx, 24(%eax)
    movl $0, 28(%eax)

    /* Map the first 4 GiB as 2 MiB pages (2048 entries across pd0..pd3). */
    mov $pd0, %eax
    mov $2048, %ecx
    mov $0x83, %edx
1:
    mov %edx, (%eax)
    movl $0, 4(%eax)
    add $0x200000, %edx
    add $8, %eax
    loop 1b

    mov $pml4, %eax
    mov %eax, %cr3

    /* Enable PAE and PGE in CR4 */
    mov %cr4, %eax
    or $0x00000020, %eax
    mov %eax, %cr4

    /* Set EFER.LME to enable long mode */
    mov $0xC0000080, %ecx
    rdmsr
    or $0x00000100, %eax
    wrmsr

    /* Enable paging and protected mode */
    mov %cr0, %eax
    or $0x80000000, %eax
    mov %eax, %cr0

    lgdt gdt64_pointer

    ljmp $0x08, $long_mode

.align 16
gdt64:
    .quad 0x0000000000000000
    .quad 0x00AF9A000000FFFF
    .quad 0x00CF93000000FFFF
    .quad 0x00CFFA000000FFFF
    .quad 0x00CFF2000000FFFF
    .quad 0x0000000000000000

gdt64_pointer:
    .word gdt64_pointer - gdt64 - 1
    .quad gdt64

.code64
long_mode:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    mov $stack_top, %rsp
    xor %rbp, %rbp

    mov %esi, %edi
    call kmain

.hang:
    cli
    hlt
    jmp .hang
