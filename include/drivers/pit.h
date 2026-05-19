#ifndef PIT_H
#define PIT_H

#include "common.h"
#include "cpu.h"

typedef void (*timer_callback_t)(registers_t *regs);

void pit_init(u32 frequency);
u32 pit_get_ticks(void);
void pit_install_callback(timer_callback_t callback);
void pit_enable(void);
void pit_disable(void);

#endif /* PIT_H */
