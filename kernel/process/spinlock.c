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
