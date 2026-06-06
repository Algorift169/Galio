/* isr.c - CPU exception handlers and interrupt dispatching */
#include "cpu.h"
#include "process.h"
#include "signals.h"
#include "paging.h"
#include "vga.h"
#include "common.h"
#include "kprintf.h"
#include <stddef.h>

/* Handlers for each interrupt - initialized to NULL */
static interrupt_handler_t handlers[256] = {0};

void interrupt_install_handler(u32 n, interrupt_handler_t handler) {
    handlers[n] = handler;
}

/* Exception names for debugging */
static const char *exception_names[] = {
    "Divide by zero", "Debug", "Non-maskable interrupt", "Breakpoint",
    "Overflow", "Bound range exceeded", "Invalid opcode", "Device not available",
    "Double fault", "Coprocessor segment overrun", "Invalid TSS", "Segment not present",
    "Stack-segment fault", "General protection fault", "Page fault", "Reserved",
    "x87 floating-point", "Alignment check", "Machine check", "SIMD floating-point",
    "Virtualization", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Security exception", "Reserved"
};

static u8 exception_has_error_code(u32 int_no) {
    return int_no == 8 || int_no == 10 || int_no == 11 || int_no == 12 ||
           int_no == 13 || int_no == 14 || int_no == 17;
}

static void print_registers(registers_t *regs) {
    vga_puts("Registers:\n");
    kprintf("  EAX=%08X  EBX=%08X  ECX=%08X  EDX=%08X\n",
            regs->eax, regs->ebx, regs->ecx, regs->edx);
    kprintf("  ESI=%08X  EDI=%08X  EBP=%08X  ESP=%08X\n",
            regs->esi, regs->edi, regs->ebp, regs->esp);
    kprintf("  EIP=%08X  EFLAGS=%08X  CS=%04X  DS=%04X\n",
            regs->eip, regs->eflags, regs->cs, regs->ds);
    if ((regs->cs & 3) == 3) {
        kprintf("  USER ESP=%08X  USER SS=%04X\n", regs->user_esp, regs->user_ss);
    }
    if (regs->interrupt_number >= 32) {
        kprintf("  Error code: %08X\n", regs->error_code);
    }
}

/* Main ISR handler - called from assembly */
void isr_handler(registers_t *regs) {
    u32 int_no = regs->interrupt_number;

    if (int_no == 0x80) {
        if (handlers[int_no] != NULL)
            handlers[int_no](regs);
        return;
    }

    /* Ignore NMI (INT 2) completely */
    if (int_no == 2) {
        return;
    }

    /* Ignore divide by zero (INT 0) completely */
    if (int_no == 0) {
        return;
    }

    if (int_no < 32) {
        vga_puts("\n=== CPU EXCEPTION ===\n");
        kprintf("Exception: %s (INT %d)\n", exception_names[int_no], int_no);
        //print_registers(regs);

        if (exception_has_error_code(int_no)) {
            kprintf("  Error code: %08X\n", regs->error_code);
        }

        if (int_no == 14) {
            u32 cr2;
            __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
            u32 present = regs->error_code & 0x1;
            u32 write = regs->error_code & 0x2;
            u32 user = regs->error_code & 0x4;
            u32 reserved = regs->error_code & 0x8;
            u32 instr = regs->error_code & 0x10;

            kprintf("Page fault at CR2=%08X\n", cr2);
            kprintf("  Cause: %s, %s, %s\n",
                    present ? "protection violation" : "non-present page",
                    write ? "write" : "read",
                    user ? "user-mode" : "kernel-mode");
            if (reserved) {
                kprintf("  Fault had reserved-bit violation\n");
            }
            if (instr) {
                kprintf("  Instruction fetch fault\n");
            }
            kprintf("  EIP=%08X  ESP=%08X  CS=%04X\n",
                    regs->eip, regs->esp, regs->cs);
            if ((regs->cs & 3) == 3) {
                kprintf("  USER ESP=%08X  USER SS=%04X\n",
                        regs->user_esp, regs->user_ss);
            }
            process_t *current = process_current();
            kprintf("  Process PID=%u\n", current ? current->pid : 0xFFFFFFFFu);

            if (paging_handle_page_fault(regs) == PAGE_FAULT_HANDLED) {
                //return;
            }
        }

        process_t *current = process_current();
        if ((regs->cs & 3) == 3 && current) {
            kprintf("Delivering SIGSEGV to PID=%u and keeping kernel alive\n", current->pid);
            process_send_signal(current->pid, SIGSEGV);
            return;
        }

        kprintf("Kernel exception ignored, continuing execution.\n");
        return;
    }

    if (handlers[int_no] != NULL) {
        handlers[int_no](regs);
    }
}

/* Main IRQ handler - called from assembly */
void irq_handler(registers_t *regs) {
    if (regs->interrupt_number >= 40)
        outb(0xA0, 0x20);
    outb(0x20, 0x20);

    if (handlers[regs->interrupt_number] != NULL)
        handlers[regs->interrupt_number](regs);
}