#ifndef IDT_H
#define IDT_H

#include "common.h"

struct idt_entry {
    u16 offset_low;
    u16 selector;
    u8 ist;
    u8 flags;
    u16 offset_mid;
    u32 offset_high;
    u32 reserved;
} __attribute__((packed));

struct idt_ptr {
    u16 limit;
    u64 base;
} __attribute__((packed));

void idt_init(void);
void idt_set_gate(int n, uintptr_t handler, u16 sel, u8 flags);

#endif /* IDT_H */
