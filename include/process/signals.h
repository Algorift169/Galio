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

#ifndef SIGNALS_H
#define SIGNALS_H

#include "process.h"
#include "common.h"

#define SIGINT   2
#define SIGKILL  9
#define SIGSEGV 11
#define SIGCHLD 17

#define SIGNAL_BIT(sig) (1u << ((sig) % 32))

u8 process_send_signal(u32 pid, u8 sig);
void process_handle_pending_signals(process_t *proc);

i32 process_waitpid(i32 pid);

#endif /* SIGNALS_H */
