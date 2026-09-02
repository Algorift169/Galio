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

#include "idt.h"
#include "common.h"
#include "vga.h"
#include "kprintf.h"

static struct idt_entry idt[256];
static struct idt_ptr idtp;

extern void idt_load(uintptr_t);

/* Declare all ISR and IRQ handlers */
extern void isr0(void), isr1(void), isr2(void), isr3(void), isr4(void);
extern void isr5(void), isr6(void), isr7(void), isr8(void), isr9(void);
extern void isr10(void), isr11(void), isr12(void), isr13(void), isr14(void);
extern void isr15(void), isr16(void), isr17(void), isr18(void), isr19(void);
extern void isr20(void), isr21(void), isr22(void), isr23(void), isr24(void);
extern void isr25(void), isr26(void), isr27(void), isr28(void), isr29(void);
extern void isr30(void), isr31(void);

extern void irq0(void), irq1(void), irq2(void), irq3(void), irq4(void);
extern void irq5(void), irq6(void), irq7(void), irq8(void), irq9(void);
extern void irq10(void), irq11(void), irq12(void), irq13(void), irq14(void), irq15(void);

static void (*isr_array[])(void) = {
    isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
    isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
};

static void (*irq_array[])(void) = {
    irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7,
    irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15
};

void idt_set_gate(int n, uintptr_t handler, u16 sel, u8 flags) {
    idt[n].offset_low = handler & 0xFFFF;
    idt[n].offset_mid = (handler >> 16) & 0xFFFF;
    idt[n].offset_high = (handler >> 32) & 0xFFFFFFFFULL;
    idt[n].selector = sel;
    idt[n].ist = 0;
    idt[n].flags = flags;
    idt[n].reserved = 0;
}

void idt_init(void) {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (uintptr_t)&idt;

    for (int i = 0; i < 256; ++i) {
        idt_set_gate(i, 0, 0x00, 0x00);
    }

    for (int i = 0; i < 32; ++i) {
        idt_set_gate(i, (uintptr_t)isr_array[i], 0x08, 0x8E);
    }

    for (int i = 0; i < 16; ++i) {
        idt_set_gate(32 + i, (uintptr_t)irq_array[i], 0x08, 0x8E);
    }

    extern void isr_syscall(void);
    idt_set_gate(0x80, (uintptr_t)isr_syscall, 0x08, 0xEE);

    idt_load((uintptr_t)&idtp);

    kprintf("IDT initialized with %d handlers\n", 48);
}