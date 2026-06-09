#ifndef CPU_SCHEDULER_H
#define CPU_SCHEDULER_H

#include "cpu.h"

void cpu_scheduler_init(void);
void cpu_scheduler_tick(registers_t *regs);

#endif /* CPU_SCHEDULER_H */
