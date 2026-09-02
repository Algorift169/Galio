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
