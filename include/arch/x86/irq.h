#ifndef IRQ_H
#define IRQ_H

#include "cpu.h"

void irq_install(void);
void irq_mask(u8 irq);
void irq_unmask(u8 irq);

#endif /* IRQ_H */
