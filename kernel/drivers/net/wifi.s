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

	.file	"wifi.c"
	.text
	.section	.rodata.str1.4,"aMS",@progbits,1
	.align 4
.LC0:
	.string	"wifi: Detected PCI device %04x:%04x at %u:%u.%u\n"
	.text
	.p2align 4
	.type	wifi_pci_probe, @function
wifi_pci_probe:
.LFB9:
	.cfi_startproc
	pushl	%ebx
	.cfi_def_cfa_offset 8
	.cfi_offset 3, -8
	call	__x86.get_pc_thunk.bx
	addl	$_GLOBAL_OFFSET_TABLE_, %ebx
	subl	$8, %esp
	.cfi_def_cfa_offset 16
	movl	16(%esp), %eax
	testl	%eax, %eax
	je	.L4
	cmpb	$2, 8(%eax)
	jne	.L4
	movzbl	2(%eax), %edx
	subl	$8, %esp
	.cfi_def_cfa_offset 24
	movb	$1, wifi_hw_present@GOTOFF(%ebx)
	pushl	%edx
	.cfi_def_cfa_offset 28
	movzbl	1(%eax), %edx
	pushl	%edx
	.cfi_def_cfa_offset 32
	movzbl	(%eax), %edx
	pushl	%edx
	.cfi_def_cfa_offset 36
	movzwl	6(%eax), %edx
	movzwl	4(%eax), %eax
	pushl	%edx
	.cfi_def_cfa_offset 40
	pushl	%eax
	.cfi_def_cfa_offset 44
	leal	.LC0@GOTOFF(%ebx), %eax
	pushl	%eax
	.cfi_def_cfa_offset 48
	call	kprintf@PLT
	addl	$32, %esp
	.cfi_def_cfa_offset 16
	xorl	%eax, %eax
.L1:
	addl	$8, %esp
	.cfi_remember_state
	.cfi_def_cfa_offset 8
	popl	%ebx
	.cfi_restore 3
	.cfi_def_cfa_offset 4
	ret
	.p2align 4,,10
	.p2align 3
.L4:
	.cfi_restore_state
	movl	$-1, %eax
	jmp	.L1
	.cfi_endproc
.LFE9:
	.size	wifi_pci_probe, .-wifi_pci_probe
	.section	.rodata.str1.1,"aMS",@progbits,1
.LC1:
	.string	"wifi: Initialized\n"
	.text
	.p2align 4
	.globl	wifi_init
	.type	wifi_init, @function
wifi_init:
.LFB11:
	.cfi_startproc
	pushl	%ebx
	.cfi_def_cfa_offset 8
	.cfi_offset 3, -8
	call	__x86.get_pc_thunk.bx
	addl	$_GLOBAL_OFFSET_TABLE_, %ebx
	subl	$20, %esp
	.cfi_def_cfa_offset 28
	leal	wifi_pci_driver@GOTOFF(%ebx), %eax
	movl	$0, wifi_device@GOTOFF(%ebx)
	pushl	%eax
	.cfi_def_cfa_offset 32
	movb	$0, wifi_hw_present@GOTOFF(%ebx)
	movl	$0, wifi_scan_count@GOTOFF(%ebx)
	call	pci_register_driver@PLT
	call	usb_init@PLT
	leal	.LC1@GOTOFF(%ebx), %eax
	movl	%eax, (%esp)
	call	kprintf@PLT
	addl	$24, %esp
	.cfi_def_cfa_offset 8
	popl	%ebx
	.cfi_restore 3
	.cfi_def_cfa_offset 4
	ret
	.cfi_endproc
.LFE11:
	.size	wifi_init, .-wifi_init
	.section	.rodata.str1.1
.LC2:
	.string	"wlan0"
	.section	.rodata.str1.4
	.align 4
.LC3:
	.string	"wifi: Registered wlan0 interface\n"
	.section	.rodata.str1.1
.LC4:
	.string	"wifi: Active scan started\n"
	.section	.rodata.str1.4
	.align 4
.LC5:
	.string	"wifi: wlan0 not present for scanning\n"
	.align 4
.LC6:
	.string	"wifi: found SSID='%s' ch=%u rssi=%d\n"
	.align 4
.LC7:
	.string	"wifi: Scan finished, %u results\n"
	.text
	.p2align 4
	.globl	wifi_scan_start
	.type	wifi_scan_start, @function
wifi_scan_start:
.LFB12:
	.cfi_startproc
	pushl	%ebp
	.cfi_def_cfa_offset 8
	.cfi_offset 5, -8
	pushl	%edi
	.cfi_def_cfa_offset 12
	.cfi_offset 7, -12
	pushl	%esi
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	pushl	%ebx
	.cfi_def_cfa_offset 20
	.cfi_offset 3, -20
	call	__x86.get_pc_thunk.bx
	addl	$_GLOBAL_OFFSET_TABLE_, %ebx
	subl	$2412, %esp
	.cfi_def_cfa_offset 2432
	movl	wifi_device@GOTOFF(%ebx), %eax
	testl	%eax, %eax
	je	.L10
	leal	.LC2@GOTOFF(%ebx), %esi
.L11:
	subl	$4, %esp
	.cfi_def_cfa_offset 2436
	leal	wifi_scan_cache@GOTOFF(%ebx), %eax
	movl	$0, wifi_scan_count@GOTOFF(%ebx)
	pushl	$1120
	.cfi_def_cfa_offset 2440
	pushl	$0
	.cfi_def_cfa_offset 2444
	movl	%eax, 36(%esp)
	pushl	%eax
	.cfi_def_cfa_offset 2448
	call	memset@PLT
	leal	.LC4@GOTOFF(%ebx), %eax
	movl	%eax, (%esp)
	call	kprintf@PLT
	movl	%esi, (%esp)
	call	netdev_get_by_name@PLT
	movl	%eax, 36(%esp)
	addl	$16, %esp
	.cfi_def_cfa_offset 2432
	testl	%eax, %eax
	je	.L56
	call	pit_get_ticks@PLT
	subl	$4, %esp
	.cfi_def_cfa_offset 2436
	movl	%eax, 40(%esp)
	pushl	$0
	.cfi_def_cfa_offset 2440
	pushl	$256
	.cfi_def_cfa_offset 2444
	leal	108(%esp), %eax
	movl	%eax, 44(%esp)
	pushl	%eax
	.cfi_def_cfa_offset 2448
	call	wifi_build_probe_request@PLT
	movzbl	%al, %eax
	movl	%eax, 44(%esp)
	addl	$16, %esp
	.cfi_def_cfa_offset 2432
	leal	wifi_scan_cache@GOTOFF, %eax
	movl	%eax, 40(%esp)
	leal	.LC6@GOTOFF(%ebx), %eax
	movl	$1, 16(%esp)
	movl	%eax, 44(%esp)
	.p2align 4
	.p2align 3
.L30:
	movl	20(%esp), %edx
	movl	60(%edx), %eax
	testl	%eax, %eax
	je	.L17
	subl	$8, %esp
	.cfi_def_cfa_offset 2440
	pushl	24(%esp)
	.cfi_def_cfa_offset 2444
	pushl	%edx
	.cfi_def_cfa_offset 2448
	call	*%eax
	addl	$16, %esp
	.cfi_def_cfa_offset 2432
.L17:
	subl	$8, %esp
	.cfi_def_cfa_offset 2440
	pushl	36(%esp)
	.cfi_def_cfa_offset 2444
	pushl	44(%esp)
	.cfi_def_cfa_offset 2448
	call	net_buf_clone_from_data@PLT
	addl	$16, %esp
	.cfi_def_cfa_offset 2432
	movl	%eax, %esi
	testl	%eax, %eax
	je	.L18
	movl	20(%esp), %ecx
	movl	44(%ecx), %eax
	testl	%eax, %eax
	je	.L19
	subl	$8, %esp
	.cfi_def_cfa_offset 2440
	pushl	%esi
	.cfi_def_cfa_offset 2444
	pushl	%ecx
	.cfi_def_cfa_offset 2448
	call	*%eax
	addl	$16, %esp
	.cfi_def_cfa_offset 2432
.L19:
	subl	$12, %esp
	.cfi_def_cfa_offset 2444
	pushl	%esi
	.cfi_def_cfa_offset 2448
	call	net_buf_free@PLT
	addl	$16, %esp
	.cfi_def_cfa_offset 2432
.L18:
	call	pit_get_ticks@PLT
	movl	%eax, 4(%esp)
	leal	352(%esp), %eax
	movl	%eax, 8(%esp)
	leal	62(%esp), %eax
	movl	%eax, 12(%esp)
	.p2align 4
	.p2align 3
.L20:
	call	pit_get_ticks@PLT
	subl	4(%esp), %eax
	cmpl	$29, %eax
	ja	.L57
.L28:
	subl	$8, %esp
	.cfi_def_cfa_offset 2440
	pushl	$50
	.cfi_def_cfa_offset 2444
	pushl	$2048
	.cfi_def_cfa_offset 2448
	movl	24(%esp), %edi
	pushl	%edi
	.cfi_def_cfa_offset 2452
	pushl	$129
	.cfi_def_cfa_offset 2456
	pushl	$0
	.cfi_def_cfa_offset 2460
	pushl	$0
	.cfi_def_cfa_offset 2464
	call	usb_bulk_read@PLT
	addl	$32, %esp
	.cfi_def_cfa_offset 2432
	testl	%eax, %eax
	jle	.L20
	subl	$12, %esp
	.cfi_def_cfa_offset 2444
	pushl	24(%esp)
	.cfi_def_cfa_offset 2448
	leal	77(%esp), %edx
	pushl	%edx
	.cfi_def_cfa_offset 2452
	leal	83(%esp), %esi
	pushl	%esi
	.cfi_def_cfa_offset 2456
	pushl	%eax
	.cfi_def_cfa_offset 2460
	pushl	%edi
	.cfi_def_cfa_offset 2464
	call	wifi_parse_beacon@PLT
	addl	$32, %esp
	.cfi_def_cfa_offset 2432
	testl	%eax, %eax
	jne	.L20
	movl	wifi_scan_count@GOTOFF(%ebx), %eax
	testl	%eax, %eax
	je	.L24
	movl	24(%esp), %edi
	xorl	%ebp, %ebp
	.p2align 4
	.p2align 3
.L26:
	subl	$8, %esp
	.cfi_def_cfa_offset 2440
	pushl	%esi
	.cfi_def_cfa_offset 2444
	pushl	%edi
	.cfi_def_cfa_offset 2448
	call	strcmp@PLT
	addl	$16, %esp
	.cfi_def_cfa_offset 2432
	testl	%eax, %eax
	je	.L20
	addl	$1, %ebp
	addl	$35, %edi
	cmpl	wifi_scan_count@GOTOFF(%ebx), %ebp
	jb	.L26
	movl	wifi_scan_count@GOTOFF(%ebx), %eax
	cmpl	$31, %eax
	ja	.L20
.L24:
	subl	$4, %esp
	.cfi_def_cfa_offset 2436
	imull	$35, %eax, %eax
	pushl	$32
	.cfi_def_cfa_offset 2440
	pushl	%esi
	.cfi_def_cfa_offset 2444
	movl	36(%esp), %ebp
	addl	%ebp, %eax
	pushl	%eax
	.cfi_def_cfa_offset 2448
	call	strncpy@PLT
	movl	wifi_scan_count@GOTOFF(%ebx), %edi
	movsbl	77(%esp), %ecx
	movzbl	78(%esp), %edx
	imull	$35, %edi, %eax
	addl	$1, %edi
	movl	%edi, wifi_scan_count@GOTOFF(%ebx)
	movb	$0, 32(%ebp,%eax)
	addl	%ebx, %eax
	addl	56(%esp), %eax
	movb	%cl, 33(%eax)
	movb	%dl, 34(%eax)
	pushl	%ecx
	.cfi_def_cfa_offset 2452
	pushl	%edx
	.cfi_def_cfa_offset 2456
	pushl	%esi
	.cfi_def_cfa_offset 2460
	pushl	72(%esp)
	.cfi_def_cfa_offset 2464
	call	kprintf@PLT
	addl	$32, %esp
	.cfi_def_cfa_offset 2432
	call	pit_get_ticks@PLT
	subl	4(%esp), %eax
	cmpl	$29, %eax
	jbe	.L28
	.p2align 4
	.p2align 3
.L57:
	call	pit_get_ticks@PLT
	movl	16(%esp), %edx
	cmpl	$14, %edx
	je	.L29
	addl	$1, %edx
	subl	36(%esp), %eax
	movl	%edx, 16(%esp)
	cmpl	$2999, %eax
	jbe	.L30
.L29:
	subl	$8, %esp
	.cfi_def_cfa_offset 2440
	leal	.LC7@GOTOFF(%ebx), %eax
	pushl	wifi_scan_count@GOTOFF(%ebx)
	.cfi_def_cfa_offset 2444
	pushl	%eax
	.cfi_def_cfa_offset 2448
	call	kprintf@PLT
	addl	$16, %esp
	.cfi_def_cfa_offset 2432
	addl	$2412, %esp
	.cfi_remember_state
	.cfi_def_cfa_offset 20
	popl	%ebx
	.cfi_restore 3
	.cfi_def_cfa_offset 16
	popl	%esi
	.cfi_restore 6
	.cfi_def_cfa_offset 12
	popl	%edi
	.cfi_restore 7
	.cfi_def_cfa_offset 8
	popl	%ebp
	.cfi_restore 5
	.cfi_def_cfa_offset 4
	ret
.L56:
	.cfi_restore_state
	subl	$12, %esp
	.cfi_def_cfa_offset 2444
	leal	.LC5@GOTOFF(%ebx), %eax
	pushl	%eax
	.cfi_def_cfa_offset 2448
	call	kprintf@PLT
	addl	$16, %esp
	.cfi_def_cfa_offset 2432
	addl	$2412, %esp
	.cfi_remember_state
	.cfi_def_cfa_offset 20
	popl	%ebx
	.cfi_restore 3
	.cfi_def_cfa_offset 16
	popl	%esi
	.cfi_restore 6
	.cfi_def_cfa_offset 12
	popl	%edi
	.cfi_restore 7
	.cfi_def_cfa_offset 8
	popl	%ebp
	.cfi_restore 5
	.cfi_def_cfa_offset 4
	ret
.L10:
	.cfi_restore_state
	subl	$12, %esp
	.cfi_def_cfa_offset 2444
	leal	.LC2@GOTOFF(%ebx), %esi
	pushl	%esi
	.cfi_def_cfa_offset 2448
	call	netdev_get_by_name@PLT
	addl	$16, %esp
	.cfi_def_cfa_offset 2432
	testl	%eax, %eax
	je	.L12
	movl	%eax, wifi_device@GOTOFF(%ebx)
	jmp	.L11
.L12:
	subl	$12, %esp
	.cfi_def_cfa_offset 2444
	pushl	$112
	.cfi_def_cfa_offset 2448
	call	kmalloc@PLT
	addl	$16, %esp
	.cfi_def_cfa_offset 2432
	movl	%eax, %edi
	testl	%eax, %eax
	je	.L11
	subl	$4, %esp
	.cfi_def_cfa_offset 2436
	pushl	$112
	.cfi_def_cfa_offset 2440
	pushl	$0
	.cfi_def_cfa_offset 2444
	pushl	%eax
	.cfi_def_cfa_offset 2448
	call	memset@PLT
	addl	$12, %esp
	.cfi_def_cfa_offset 2436
	pushl	$15
	.cfi_def_cfa_offset 2440
	pushl	%esi
	.cfi_def_cfa_offset 2444
	pushl	%edi
	.cfi_def_cfa_offset 2448
	call	strncpy@PLT
	movl	$1500, 24(%edi)
	movl	$0, 28(%edi)
	movl	$0, 32(%edi)
	movl	$0, 44(%edi)
	movl	%edi, (%esp)
	call	netdev_register@PLT
	addl	$16, %esp
	.cfi_def_cfa_offset 2432
	testl	%eax, %eax
	jne	.L58
	subl	$12, %esp
	.cfi_def_cfa_offset 2444
	leal	.LC3@GOTOFF(%ebx), %eax
	movl	%edi, wifi_device@GOTOFF(%ebx)
	pushl	%eax
	.cfi_def_cfa_offset 2448
	call	kprintf@PLT
	addl	$16, %esp
	.cfi_def_cfa_offset 2432
	jmp	.L11
.L58:
	subl	$12, %esp
	.cfi_def_cfa_offset 2444
	pushl	%edi
	.cfi_def_cfa_offset 2448
	call	kfree@PLT
	addl	$16, %esp
	.cfi_def_cfa_offset 2432
	jmp	.L11
	.cfi_endproc
.LFE12:
	.size	wifi_scan_start, .-wifi_scan_start
	.section	.rodata.str1.1
.LC8:
	.string	"wifi: Scan stopped\n"
	.text
	.p2align 4
	.globl	wifi_scan_stop
	.type	wifi_scan_stop, @function
wifi_scan_stop:
.LFB13:
	.cfi_startproc
	pushl	%ebx
	.cfi_def_cfa_offset 8
	.cfi_offset 3, -8
	call	__x86.get_pc_thunk.bx
	addl	$_GLOBAL_OFFSET_TABLE_, %ebx
	subl	$20, %esp
	.cfi_def_cfa_offset 28
	leal	.LC8@GOTOFF(%ebx), %eax
	pushl	%eax
	.cfi_def_cfa_offset 32
	call	kprintf@PLT
	addl	$24, %esp
	.cfi_def_cfa_offset 8
	popl	%ebx
	.cfi_restore 3
	.cfi_def_cfa_offset 4
	ret
	.cfi_endproc
.LFE13:
	.size	wifi_scan_stop, .-wifi_scan_stop
	.p2align 4
	.globl	wifi_scan_results
	.type	wifi_scan_results, @function
wifi_scan_results:
.LFB14:
	.cfi_startproc
	call	__x86.get_pc_thunk.ax
	addl	$_GLOBAL_OFFSET_TABLE_, %eax
	movl	4(%esp), %edx
	testl	%edx, %edx
	je	.L62
	movl	wifi_scan_count@GOTOFF(%eax), %ecx
	movl	%ecx, (%edx)
.L62:
	leal	wifi_scan_cache@GOTOFF(%eax), %eax
	ret
	.cfi_endproc
.LFE14:
	.size	wifi_scan_results, .-wifi_scan_results
	.p2align 4
	.globl	wifi_has_hardware
	.type	wifi_has_hardware, @function
wifi_has_hardware:
.LFB15:
	.cfi_startproc
	call	__x86.get_pc_thunk.ax
	addl	$_GLOBAL_OFFSET_TABLE_, %eax
	movzbl	wifi_hw_present@GOTOFF(%eax), %eax
	ret
	.cfi_endproc
.LFE15:
	.size	wifi_has_hardware, .-wifi_has_hardware
	.p2align 4
	.globl	wifi_parse_80211_beacon
	.type	wifi_parse_80211_beacon, @function
wifi_parse_80211_beacon:
.LFB16:
	.cfi_startproc
	movl	$-1, %eax
	ret
	.cfi_endproc
.LFE16:
	.size	wifi_parse_80211_beacon, .-wifi_parse_80211_beacon
	.section	.data.rel.local,"aw"
	.align 4
	.type	wifi_pci_driver, @object
	.size	wifi_pci_driver, 12
wifi_pci_driver:
	.value	-1
	.value	-1
	.long	wifi_pci_probe
	.long	0
	.local	wifi_device
	.comm	wifi_device,4,4
	.local	wifi_hw_present
	.comm	wifi_hw_present,1,1
	.local	wifi_scan_count
	.comm	wifi_scan_count,4,4
	.local	wifi_scan_cache
	.comm	wifi_scan_cache,1120,32
	.section	.text.__x86.get_pc_thunk.ax,"axG",@progbits,__x86.get_pc_thunk.ax,comdat
	.globl	__x86.get_pc_thunk.ax
	.hidden	__x86.get_pc_thunk.ax
	.type	__x86.get_pc_thunk.ax, @function
__x86.get_pc_thunk.ax:
.LFB17:
	.cfi_startproc
	movl	(%esp), %eax
	ret
	.cfi_endproc
.LFE17:
	.section	.text.__x86.get_pc_thunk.bx,"axG",@progbits,__x86.get_pc_thunk.bx,comdat
	.globl	__x86.get_pc_thunk.bx
	.hidden	__x86.get_pc_thunk.bx
	.type	__x86.get_pc_thunk.bx, @function
__x86.get_pc_thunk.bx:
.LFB18:
	.cfi_startproc
	movl	(%esp), %ebx
	ret
	.cfi_endproc
.LFE18:
	.ident	"GCC: (Debian 15.2.0-17) 15.2.0"
	.section	.note.GNU-stack,"",@progbits
