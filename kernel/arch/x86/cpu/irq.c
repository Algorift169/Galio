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

#include "irq.h"
#include "vga.h"
#include "common.h"
#include "kprintf.h"
#include "cpu.h"

/* PIC ports */
#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

/* ICW1: Initialization Control Word 1 */
#define ICW1_ICW4       0x01
#define ICW1_SINGLE     0x02
#define ICW1_INTERVAL4  0x04
#define ICW1_LEVEL      0x08
#define ICW1_INIT       0x10

/* ICW4: Initialization Control Word 4 */
#define ICW4_8086       0x01
#define ICW4_AUTO       0x02
#define ICW4_BUF_SLAVE  0x08
#define ICW4_BUF_MASTER 0x0C
#define ICW4_SFNM       0x10

/* Remap PIC to avoid conflicts with CPU exceptions */
static void pic_remap(void) {
    u8 a1, a2;

    a1 = inb(PIC1_DATA);
    a2 = inb(PIC2_DATA);

    /* ICW1 */
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    
    /* ICW2 - vector offset */
    outb(PIC1_DATA, 0x20); /* Master offset 0x20 (ISR 32-39) */
    outb(PIC2_DATA, 0x28); /* Slave offset 0x28 (ISR 40-47) */
    
    /* ICW3 - cascade */
    outb(PIC1_DATA, 0x04); /* IR2 has slave PIC */
    outb(PIC2_DATA, 0x02); /* Slave is on IR2 */
    
    /* ICW4 - 8086 mode */
    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);

    /* OCW1 - restore masks */
    outb(PIC1_DATA, a1);
    outb(PIC2_DATA, a2);
}

void irq_install(void) {
    pic_remap();
    
    /* Enable IRQ0 (timer) and IRQ1 (keyboard) by default */
    u8 mask = inb(PIC1_DATA);
    mask &= ~0x03;  /* Enable IRQ0 and IRQ1 */
    outb(PIC1_DATA, mask);
    
    kprintf("IRQ/PIC remapped, timer and keyboard enabled\n");
}

/* Unmask a specific IRQ */
void irq_unmask(u8 irq) {
    u16 port;
    u8 mask;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }

    mask = inb(port);
    mask &= ~(1 << irq);
    outb(port, mask);
}

/* Mask a specific IRQ */
void irq_mask(u8 irq) {
    u16 port;
    u8 mask;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }

    mask = inb(port);
    mask |= (1 << irq);
    outb(port, mask);
}