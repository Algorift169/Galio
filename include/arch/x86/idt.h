#ifndef IDT_H
#define IDT_H

#include "common.h"

struct idt_entry {
    u16 base_lo;
    u16 sel;
    u8 always0;
    u8 flags;
    u16 base_hi;
} __attribute__((packed));

struct idt_ptr {
    u16 limit;
    u32 base;
} __attribute__((packed));

void idt_init(void);
void idt_set_gate(int n, u32 handler, u16 sel, u8 flags);

#endif /* IDT_H */
