/* isr.c - CPU exception handlers and interrupt dispatching */
#include "cpu.h"
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

static void print_registers(registers_t *regs) {
    vga_puts("Registers:\n");
    kprintf("  EAX=%08X  EBX=%08X  ECX=%08X  EDX=%08X\n",
            regs->eax, regs->ebx, regs->ecx, regs->edx);
    kprintf("  ESI=%08X  EDI=%08X  EBP=%08X  ESP=%08X\n",
            regs->esi, regs->edi, regs->ebp, regs->esp);
    kprintf("  EIP=%08X  EFLAGS=%08X  CS=%04X  DS=%04X\n",
            regs->eip, regs->eflags, regs->cs, regs->ds);
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
    
    if (int_no < 32) {
        vga_puts("\n=== CPU EXCEPTION ===\n");
        kprintf("Exception: %s (INT %d)\n", exception_names[int_no], int_no);
        print_registers(regs);
        if (int_no == 14) {
            u32 cr2;
            __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
            kprintf("Faulting address: %08X\n", cr2);
        }
        vga_puts("=== HALTED ===\n");
        disable_interrupts();
        halt();
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