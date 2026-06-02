/* Simple spinlock for process table protection */
#ifndef PROCESS_SPINLOCK_H
#define PROCESS_SPINLOCK_H

#include "common.h"

typedef struct {
    volatile int locked;
} spinlock_t;

void spinlock_init(spinlock_t *lock);
void spin_lock(spinlock_t *lock);
void spin_unlock(spinlock_t *lock);

#endif /* PROCESS_SPINLOCK_H */
