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

static int freezer_enabled = 1;
static int freezer_active = 0;

void power_freezer_enable(void)
{
    freezer_enabled = 1;
}

void power_freezer_disable(void)
{
    freezer_enabled = 0;
}

int power_freezer_is_active(void)
{
    return freezer_active;
}

void power_freezer_activate(void)
{
    freezer_active = freezer_enabled;
    kprintf("[POWER] freezer activated\n");
}

void power_freezer_deactivate(void)
{
    freezer_active = 0;
    kprintf("[POWER] freezer deactivated\n");
}
