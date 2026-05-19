#ifndef PROCESS_PREEMPT_H
#define PROCESS_PREEMPT_H

#include "cpu.h"

#define PROCESS_TIME_SLICE 10

void process_preempt(registers_t *regs);

#endif /* PROCESS_PREEMPT_H */
