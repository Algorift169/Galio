/* pit.c - Programmable Interval Timer */
#include "pit.h"
#include "irq.h"
#include "kprintf.h"
#include "common.h"
#include <stddef.h>

#define PIT_FREQUENCY 1193182
#define PIT_CHANNEL0  0x40
#define PIT_CONTROL   0x43

static u32 ticks = 0;
static timer_callback_t user_callback = NULL;

/* IRQ0 handler */
static void pit_handler(registers_t *regs) {
    (void)regs;
    ticks++;

    // Debug: show ticks every second (100 Hz PIT)
    // if (ticks % 100 == 0) {
    //     kprintf("[debug] PIT tick=%u\n", ticks);
    // }

    if (user_callback) {
        user_callback(regs);
    }
}

void pit_init(u32 frequency) {
    u16 divisor = PIT_FREQUENCY / frequency;

    kprintf("PIT: Setting frequency to %u Hz (divisor %u)\n", frequency, divisor);

    /* Set PIT to mode 2 (rate generator) */
    outb(PIT_CONTROL, 0x36);   // <-- fixed: correct control word

    /* Set divisor */
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);

    /* Install IRQ0 handler */
    /* Install IRQ0 handler */
    interrupt_install_handler(32, pit_handler);


    /* Unmask IRQ0 */
    irq_unmask(0);
}

u32 pit_get_ticks(void) {
    return ticks;
}

void pit_install_callback(timer_callback_t callback) {
    user_callback = callback;
}

void pit_enable(void) {
    irq_unmask(0);
}

void pit_disable(void) {
    irq_mask(0);
}