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

/* pit.c - Programmable Interval Timer */
#include "pit.h"
#include "irq.h"
#include "kprintf.h"
#include "common.h"
#include "time/galio_time.h"
#include <stddef.h>

#define PIT_INPUT_FREQUENCY 1193182u
#define PIT_CHANNEL0  0x40
#define PIT_CONTROL   0x43

static u32 ticks = 0;
static u8 external_tick_source;
#define MAX_TIMER_CALLBACKS 8
static timer_callback_t timer_callbacks[MAX_TIMER_CALLBACKS] = {0};

/* IRQ0 handler */
static void pit_handler(registers_t *regs) {
    (void)regs;
    ticks++;

    for (u32 i = 0; i < MAX_TIMER_CALLBACKS; i++) {
        if (timer_callbacks[i]) {
            timer_callbacks[i](regs);
        }
    }
}

void pit_init(u32 frequency) {
    if (frequency == 0u) {
        frequency = GALIO_HZ;
    }

    u32 divisor_value = PIT_INPUT_FREQUENCY / frequency;
    if (divisor_value == 0u) {
        divisor_value = 1u;
    }
    if (divisor_value > 0xFFFFu) {
        divisor_value = 0xFFFFu;
    }
    u16 divisor = (u16)divisor_value;

    kprintf("PIT: Setting frequency to %u Hz (divisor %u)\n", frequency, divisor);

    /* Set PIT to mode 2 (rate generator) */
    outb(PIT_CONTROL, 0x36);   // <-- fixed: correct control word

    /* Set divisor */
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);

    /* Route IRQ0 through the active PIC or IOAPIC implementation. */
    irq_register_handler(0u, pit_handler);

    __asm__ volatile ("sti");
}

void pit_use_external_tick(void) {
    /* Stop the PIT rate generator; IRQ0 is subsequently driven by HPET legacy
       replacement mode while retaining the existing callback fan-out. */
    outb(PIT_CONTROL, 0x30);
    outb(PIT_CHANNEL0, 0u);
    outb(PIT_CHANNEL0, 0u);
    external_tick_source = 1u;
}

u32 pit_get_ticks(void) {
    return ticks;
}

void pit_install_callback(timer_callback_t callback) {
    if (!callback) return;
    for (u32 i = 0; i < MAX_TIMER_CALLBACKS; i++) {
        if (timer_callbacks[i] == callback) return;
        if (!timer_callbacks[i]) {
            timer_callbacks[i] = callback;
            return;
        }
    }
}

void pit_enable(void) {
    irq_unmask(0);
}

void pit_disable(void) {
    irq_mask(0);
}