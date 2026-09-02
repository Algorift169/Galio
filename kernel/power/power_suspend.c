/*
 * Galio Kernel
 *
 * Copyright (C) 2026 S.M Israfil
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
#include <stdbool.h>

const char *mem_sleep_labels[PM_SUSPEND_MAX] = {
    [PM_SUSPEND_TO_IDLE] = "s2idle",
    [PM_SUSPEND_STANDBY] = "shallow",
    [PM_SUSPEND_MEM] = "deep",
};

static const struct platform_suspend_ops *suspend_ops;

static bool valid_state(suspend_state_t state)
{
    return suspend_ops && suspend_ops->valid && suspend_ops->valid(state) && suspend_ops->enter;
}

bool pm_suspend_default_s2idle(void)
{
    return mem_sleep_current == PM_SUSPEND_TO_IDLE;
}

int suspend_valid_only_mem(suspend_state_t state)
{
    return state == PM_SUSPEND_MEM;
}

void suspend_set_ops(const struct platform_suspend_ops *ops)
{
    suspend_ops = ops;

    if (valid_state(PM_SUSPEND_STANDBY)) {
        mem_sleep_states[PM_SUSPEND_STANDBY] = mem_sleep_labels[PM_SUSPEND_STANDBY];
        pm_states[PM_SUSPEND_STANDBY] = pm_labels[PM_SUSPEND_STANDBY];
        if (mem_sleep_default == PM_SUSPEND_STANDBY)
            mem_sleep_current = PM_SUSPEND_STANDBY;
    }

    if (valid_state(PM_SUSPEND_MEM)) {
        mem_sleep_states[PM_SUSPEND_MEM] = mem_sleep_labels[PM_SUSPEND_MEM];
        if (mem_sleep_default >= PM_SUSPEND_MEM)
            mem_sleep_current = PM_SUSPEND_MEM;
    }
}

static void power_suspend_state_init(void)
{
    pm_states[PM_SUSPEND_MEM] = pm_labels[PM_SUSPEND_MEM];
    pm_states[PM_SUSPEND_TO_IDLE] = pm_labels[PM_SUSPEND_TO_IDLE];
    mem_sleep_states[PM_SUSPEND_TO_IDLE] = mem_sleep_labels[PM_SUSPEND_TO_IDLE];
}

void power_suspend_init(void)
{
    power_suspend_state_init();
    kprintf("[POWER] suspend framework initialized\n");
}

int power_suspend_enter(suspend_state_t state)
{
    kprintf("[POWER] suspend enter state=%d\n", state);
    return 0;
}
