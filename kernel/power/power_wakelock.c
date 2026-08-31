#include "power/power.h"
#include "kprintf.h"

static int wakelock_count = 0;

void power_wakelock_acquire(void)
{
    wakelock_count++;
    kprintf("[POWER] wakelock acquire (%d)\n", wakelock_count);
}

void power_wakelock_release(void)
{
    if (wakelock_count > 0)
        wakelock_count--;
    kprintf("[POWER] wakelock release (%d)\n", wakelock_count);
}

int power_wakelock_count_get(void)
{
    return wakelock_count;
}
