#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "cpu.h"

void scheduler_init(void);
void scheduler_tick(registers_t *regs);

#endif /* SCHEDULER_H */
