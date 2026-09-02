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

#include "power/power.h"
#include "kprintf.h"
#include "string.h"
#include "arch/x86/cpu.h"
#include <stdbool.h>

static unsigned int saved_gfp_count;
static unsigned long saved_gfp_mask;

void pm_restore_gfp_mask(void)
{
    if (!saved_gfp_count || --saved_gfp_count)
        return;

    /* The kernel uses a restricted page-allocation mask while sleeping. */
    saved_gfp_mask = 0;
}

void pm_restrict_gfp_mask(void)
{
    if (saved_gfp_count++)
        return;

    saved_gfp_mask = 0;
}

unsigned int lock_system_sleep(void)
{
    return 0;
}

void unlock_system_sleep(unsigned int flags)
{
    (void)flags;
}

int pm_notifier_call_chain(unsigned long val)
{
    (void)val;
    return 0;
}

int pm_notifier_call_chain_robust(unsigned long val_up, unsigned long val_down)
{
    (void)val_up;
    (void)val_down;
    return 0;
}

void poweroff_trigger(void)
{
    kprintf("[POWER] poweroff requested\n");
}

void poweroff_force(void)
{
    kprintf("[POWER] forced shutdown path\n");
}

void power_system_reset(void)
{
    kprintf("[POWER] reboot requested - initiating reset sequence\n");
    for (int i = 0; i < 100000; i++) {
        asm volatile("nop");
    }
    outb(0xCF9, 0x02);
    outb(0xCF9, 0x06);
    while (1) {
        asm volatile("hlt");
    }
}

void power_system_shutdown(void)
{
    kprintf("[POWER] shutdown requested - halting system\n");
    asm volatile("cli");
    while (1) {
        asm volatile("hlt");
    }
}

int power_system_suspend(void)
{
    kprintf("[POWER] suspend requested - entering sleep state\n");
    return power_suspend_enter(PM_SUSPEND_MEM);
}

const char *const pm_labels[] = {
    [PM_SUSPEND_TO_IDLE] = "freeze",
    [PM_SUSPEND_STANDBY] = "standby",
    [PM_SUSPEND_MEM] = "mem",
};

const char *pm_states[PM_SUSPEND_MAX] = { 0 };
const char *mem_sleep_states[PM_SUSPEND_MAX] = { 0 };

suspend_state_t mem_sleep_current = PM_SUSPEND_TO_IDLE;
suspend_state_t mem_sleep_default = PM_SUSPEND_MAX;
suspend_state_t pm_suspend_target_state = PM_SUSPEND_ON;
unsigned int pm_suspend_global_flags = 0;
int pm_async_enabled = 1;
