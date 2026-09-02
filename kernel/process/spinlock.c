/*
 * Galio Kernel
 *
 * Copyright (C) 2026 Israfil [Your Legal Name]
 *
 * This file is part of Galio.
 *
 * Galio is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * Galio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Galio. If not, see <https://www.gnu.org/licenses/>.
 */

/* spinlock.c - Simple x86 spinlock implementation for process table
 * Uses atomic xchg to acquire the lock. Kept minimal and documented.
 */
#include "spinlock.h"

static inline int xchg_int(volatile int *ptr, int val) {
    int old;
    __asm__ volatile("xchgl %0, %1" : "=r"(old), "+m"(*ptr) : "0"(val) : "memory");
    return old;
}

void spinlock_init(spinlock_t *lock) {
    if (!lock) return;
    lock->locked = 0;
}

void spin_lock(spinlock_t *lock) {
    if (!lock) return;
    while (xchg_int(&lock->locked, 1)) {
        /* busy-wait until unlocked */
        while (lock->locked) {
            __asm__ volatile("pause");
        }
    }
}

// Release the lock by setting locked to 0. Use sfence to ensure memory ordering.  
void spin_unlock(spinlock_t *lock) {
    if (!lock) return;
    /* Ensure memory operations are ordered before releasing lock */
    __asm__ volatile("sfence" ::: "memory");
    lock->locked = 0;
}
