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

/* isr.c - CPU exception handlers and interrupt dispatching */
#include "cpu.h"
#include "process.h"
#include "signals.h"
#include "paging.h"
#include "vga.h"
#include "common.h"
#include "kprintf.h"
#include "drivers/msr.h"
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
    kprintf("  RAX=%016llX  RBX=%016llX  RCX=%016llX  RDX=%016llX\n",
            (unsigned long long)regs->rax, (unsigned long long)regs->rbx,
            (unsigned long long)regs->rcx, (unsigned long long)regs->rdx);
    kprintf("  RSI=%016llX  RDI=%016llX  RBP=%016llX  RSP=%016llX\n",
            (unsigned long long)regs->rsi, (unsigned long long)regs->rdi,
            (unsigned long long)regs->rbp, (unsigned long long)regs->rsp);
    kprintf("  RIP=%016llX  RFLAGS=%016llX  CS=%04llX  SS=%04llX\n",
            (unsigned long long)regs->eip, (unsigned long long)regs->eflags,
            (unsigned long long)regs->cs, (unsigned long long)regs->ss);
    if ((regs->cs & 3) == 3) {
        kprintf("  USER RSP=%016llX  USER SS=%04llX\n",
                (unsigned long long)regs->rsp,
                (unsigned long long)regs->ss);
    }
    if (regs->interrupt_number >= 32) {
        kprintf("  Error code: %016llX\n", (unsigned long long)regs->error_code);
    }
}

/* Main ISR handler - called from assembly */
void isr_handler(registers_t *regs) {
    u32 int_no = regs->interrupt_number;

    if (int_no == 13) {
        if (msr_handle_general_protection(regs))
            return;
    }

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
        print_registers(regs);

        if (exception_has_error_code(int_no)) {
            kprintf("  Error code: %016llX\n", (unsigned long long)regs->error_code);
        }

        if (int_no == 14) {
            uintptr_t cr2;
            __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
            uintptr_t present = regs->error_code & 0x1;
            uintptr_t write = regs->error_code & 0x2;
            uintptr_t user = regs->error_code & 0x4;
            uintptr_t reserved = regs->error_code & 0x8;
            uintptr_t instr = regs->error_code & 0x10;

            kprintf("Page fault at CR2=%016llX\n", (unsigned long long)cr2);
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
            process_t *current = process_current();
            kprintf("  Process PID=%u\n", current ? current->pid : 0xFFFFFFFFu);

            if (paging_handle_page_fault(regs) == PAGE_FAULT_HANDLED) {
                return;
            }
        }

        process_t *current = process_current();
        if ((regs->cs & 3) == 3 && current) {
            kprintf("Delivering SIGSEGV to PID=%u\n", current->pid);
            process_send_signal(current->pid, SIGSEGV);
            return;
        }

        kprintf("KERNEL PANIC: Unhandled exception in kernel mode. Halting.\n");
        __asm__ volatile("cli");
        while(1) {
            __asm__ volatile("hlt");
        }
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