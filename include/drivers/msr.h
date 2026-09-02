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

#ifndef GALIO_MSR_H
#define GALIO_MSR_H

#include "common.h"
#include "arch/x86/cpu.h"

#define MSR_IA32_APERF 0x000000E8u
#define MSR_IA32_MPERF 0x000000E7u

#define MSR_OK 0
#define MSR_ERR_UNSUPPORTED (-1)
#define MSR_ERR_INVALID (-2)
#define MSR_ERR_FAULT (-3)

i32 msr_read(u32 index, u64 *value);
i32 msr_write(u32 index, u64 value);

/* Called by the #GP handler when an optional MSR probe faults. */
u8 msr_handle_general_protection(registers_t *regs);

#endif /* GALIO_MSR_H */