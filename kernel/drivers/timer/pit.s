/*
 * Galio Kernel
 *
 * Copyright (C) 2026 S.M Israfil
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

	.file	"pit.c"
	.text
	.p2align 4
	.type	pit_handler, @function
pit_handler:
.LFB9:
	.cfi_startproc
	call	__x86.get_pc_thunk.ax
	addl	$_GLOBAL_OFFSET_TABLE_, %eax
	pushl	%edi
	.cfi_def_cfa_offset 8
	.cfi_offset 7, -8
	pushl	%esi
	.cfi_def_cfa_offset 12
	.cfi_offset 6, -12
	pushl	%ebx
	.cfi_def_cfa_offset 16
	.cfi_offset 3, -16
	movl	16(%esp), %edi
	addl	$1, ticks@GOTOFF(%eax)
	leal	timer_callbacks@GOTOFF(%eax), %ebx
	leal	32(%ebx), %esi
	.p2align 4
	.p2align 3
.L3:
	movl	(%ebx), %eax
	testl	%eax, %eax
	je	.L2
	subl	$12, %esp
	.cfi_def_cfa_offset 28
	pushl	%edi
	.cfi_def_cfa_offset 32
	call	*%eax
	addl	$16, %esp
	.cfi_def_cfa_offset 16
.L2:
	addl	$4, %ebx
	cmpl	%esi, %ebx
	jne	.L3
	popl	%ebx
	.cfi_restore 3
	.cfi_def_cfa_offset 12
	popl	%esi
	.cfi_restore 6
	.cfi_def_cfa_offset 8
	popl	%edi
	.cfi_restore 7
	.cfi_def_cfa_offset 4
	ret
	.cfi_endproc
.LFE9:
	.size	pit_handler, .-pit_handler
	.section	.rodata.str1.4,"aMS",@progbits,1
	.align 4
.LC0:
	.string	"PIT: Setting frequency to %u Hz (divisor %u)\n"
	.text
	.p2align 4
	.globl	pit_init
	.type	pit_init, @function
pit_init:
.LFB10:
	.cfi_startproc
	pushl	%esi
	.cfi_def_cfa_offset 8
	.cfi_offset 6, -8
	xorl	%edx, %edx
	movl	$1193182, %eax
	pushl	%ebx
	.cfi_def_cfa_offset 12
	.cfi_offset 3, -12
	call	__x86.get_pc_thunk.bx
	addl	$_GLOBAL_OFFSET_TABLE_, %ebx
	subl	$8, %esp
	.cfi_def_cfa_offset 20
	movl	20(%esp), %ecx
	divl	%ecx
	movl	%eax, %esi
	movzwl	%ax, %eax
	pushl	%eax
	.cfi_def_cfa_offset 24
	leal	.LC0@GOTOFF(%ebx), %eax
	pushl	%ecx
	.cfi_def_cfa_offset 28
	pushl	%eax
	.cfi_def_cfa_offset 32
	call	kprintf@PLT
	movl	$54, %eax
#APP
# 53 "include/arch/x86/cpu.h" 1
	outb %al, $67
# 0 "" 2
#NO_APP
	movl	%esi, %eax
#APP
# 53 "include/arch/x86/cpu.h" 1
	outb %al, $64
# 0 "" 2
#NO_APP
	movl	%esi, %eax
	shrw	$8, %ax
#APP
# 53 "include/arch/x86/cpu.h" 1
	outb %al, $64
# 0 "" 2
#NO_APP
	popl	%eax
	.cfi_def_cfa_offset 28
	leal	pit_handler@GOTOFF(%ebx), %eax
	popl	%edx
	.cfi_def_cfa_offset 24
	pushl	%eax
	.cfi_def_cfa_offset 28
	pushl	$32
	.cfi_def_cfa_offset 32
	call	interrupt_install_handler@PLT
	movl	$0, (%esp)
	call	irq_unmask@PLT
	addl	$20, %esp
	.cfi_def_cfa_offset 12
	popl	%ebx
	.cfi_restore 3
	.cfi_def_cfa_offset 8
	popl	%esi
	.cfi_restore 6
	.cfi_def_cfa_offset 4
	ret
	.cfi_endproc
.LFE10:
	.size	pit_init, .-pit_init
	.p2align 4
	.globl	pit_get_ticks
	.type	pit_get_ticks, @function
pit_get_ticks:
.LFB11:
	.cfi_startproc
	call	__x86.get_pc_thunk.ax
	addl	$_GLOBAL_OFFSET_TABLE_, %eax
	movl	ticks@GOTOFF(%eax), %eax
	ret
	.cfi_endproc
.LFE11:
	.size	pit_get_ticks, .-pit_get_ticks
	.p2align 4
	.globl	pit_install_callback
	.type	pit_install_callback, @function
pit_install_callback:
.LFB12:
	.cfi_startproc
	pushl	%ebx
	.cfi_def_cfa_offset 8
	.cfi_offset 3, -8
	movl	8(%esp), %ecx
	call	__x86.get_pc_thunk.bx
	addl	$_GLOBAL_OFFSET_TABLE_, %ebx
	testl	%ecx, %ecx
	je	.L13
	xorl	%eax, %eax
	.p2align 4
	.p2align 3
.L16:
	movl	timer_callbacks@GOTOFF(%ebx,%eax,4), %edx
	cmpl	%ecx, %edx
	je	.L13
	testl	%edx, %edx
	je	.L22
	addl	$1, %eax
	cmpl	$8, %eax
	jne	.L16
.L13:
	popl	%ebx
	.cfi_remember_state
	.cfi_restore 3
	.cfi_def_cfa_offset 4
	ret
	.p2align 4,,10
	.p2align 3
.L22:
	.cfi_restore_state
	movl	%ecx, timer_callbacks@GOTOFF(%ebx,%eax,4)
	popl	%ebx
	.cfi_restore 3
	.cfi_def_cfa_offset 4
	ret
	.cfi_endproc
.LFE12:
	.size	pit_install_callback, .-pit_install_callback
	.p2align 4
	.globl	pit_enable
	.type	pit_enable, @function
pit_enable:
.LFB13:
	.cfi_startproc
	pushl	%ebx
	.cfi_def_cfa_offset 8
	.cfi_offset 3, -8
	call	__x86.get_pc_thunk.bx
	addl	$_GLOBAL_OFFSET_TABLE_, %ebx
	subl	$20, %esp
	.cfi_def_cfa_offset 28
	pushl	$0
	.cfi_def_cfa_offset 32
	call	irq_unmask@PLT
	addl	$24, %esp
	.cfi_def_cfa_offset 8
	popl	%ebx
	.cfi_restore 3
	.cfi_def_cfa_offset 4
	ret
	.cfi_endproc
.LFE13:
	.size	pit_enable, .-pit_enable
	.p2align 4
	.globl	pit_disable
	.type	pit_disable, @function
pit_disable:
.LFB14:
	.cfi_startproc
	pushl	%ebx
	.cfi_def_cfa_offset 8
	.cfi_offset 3, -8
	call	__x86.get_pc_thunk.bx
	addl	$_GLOBAL_OFFSET_TABLE_, %ebx
	subl	$20, %esp
	.cfi_def_cfa_offset 28
	pushl	$0
	.cfi_def_cfa_offset 32
	call	irq_mask@PLT
	addl	$24, %esp
	.cfi_def_cfa_offset 8
	popl	%ebx
	.cfi_restore 3
	.cfi_def_cfa_offset 4
	ret
	.cfi_endproc
.LFE14:
	.size	pit_disable, .-pit_disable
	.local	timer_callbacks
	.comm	timer_callbacks,32,32
	.local	ticks
	.comm	ticks,4,4
	.section	.text.__x86.get_pc_thunk.ax,"axG",@progbits,__x86.get_pc_thunk.ax,comdat
	.globl	__x86.get_pc_thunk.ax
	.hidden	__x86.get_pc_thunk.ax
	.type	__x86.get_pc_thunk.ax, @function
__x86.get_pc_thunk.ax:
.LFB15:
	.cfi_startproc
	movl	(%esp), %eax
	ret
	.cfi_endproc
.LFE15:
	.section	.text.__x86.get_pc_thunk.bx,"axG",@progbits,__x86.get_pc_thunk.bx,comdat
	.globl	__x86.get_pc_thunk.bx
	.hidden	__x86.get_pc_thunk.bx
	.type	__x86.get_pc_thunk.bx, @function
__x86.get_pc_thunk.bx:
.LFB16:
	.cfi_startproc
	movl	(%esp), %ebx
	ret
	.cfi_endproc
.LFE16:
	.ident	"GCC: (Debian 15.2.0-17) 15.2.0"
	.section	.note.GNU-stack,"",@progbits
