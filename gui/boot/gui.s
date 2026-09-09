; Galio GUI boot handoff.
; Called by kmain after kernel services and authentication are ready.

BITS 64

GLOBAL gui_boot
EXTERN display_enter_userland_mode

SECTION .text

gui_boot:
	push rbp
	mov rbp, rsp
	call display_enter_userland_mode
	pop rbp
	ret
