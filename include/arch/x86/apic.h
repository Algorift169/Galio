#ifndef GALIO_APIC_H
#define GALIO_APIC_H

#include "common.h"
#include "arch/x86/cpu.h"

void apic_init(void);
u8 apic_is_available(void);
void apic_eoi(void);
void apic_register_irq(u8 irq, interrupt_handler_t handler);
u32 apic_cpu_id(void);

#endif /* GALIO_APIC_H */