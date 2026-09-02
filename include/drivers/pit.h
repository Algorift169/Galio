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
