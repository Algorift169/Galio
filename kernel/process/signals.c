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

/* signals.c - Process signal delivery and waitpid support */
#include <stddef.h>
#include "signals.h"
#include "process.h"
#include "kprintf.h"
#include "cpu.h"

static u32 signal_bit(u8 sig) {
    switch (sig) {
        case SIGINT: return SIGNAL_BIT(SIGINT);
        case SIGKILL: return SIGNAL_BIT(SIGKILL);
        case SIGSEGV: return SIGNAL_BIT(SIGSEGV);
        case SIGCHLD: return SIGNAL_BIT(SIGCHLD);
        default: return 0;
    }
}

static void process_terminate(process_t *proc, u8 sig) {
    if (!proc || proc->state == PROCESS_ZOMBIE) {
        return;
    }

    if (proc->pid == 1 && sig == SIGKILL) {
        return; /* preserve idle process */
    }

    proc->exit_code = sig;
    proc->pending_signals = 0;
    proc->state = PROCESS_ZOMBIE;

    if (proc->parent_pid != 0 && proc->parent_pid != proc->pid) {
        process_send_signal(proc->parent_pid, SIGCHLD);
    }
}

u8 process_send_signal(u32 pid, u8 sig) {
    process_t *target = process_get_any(pid);
    if (!target) {
        return 0;
    }

    u32 mask = signal_bit(sig);
    if (mask == 0) {
        return 0;
    }

    if (sig == SIGCHLD) {
        target->pending_signals |= mask;
        if (target->state == PROCESS_WAITING) {
            i32 waiting_pid = target->waiting_for_pid;
            process_t *source = process_current();
            if (waiting_pid == -1 || (source && waiting_pid == (i32)source->pid)) {
                target->state = PROCESS_READY;
            }
        }
        return 1;
    }

    target->pending_signals |= mask;
    if (target == process_current()) {
        process_handle_pending_signals(target);
    }
    return 1;
}

void process_handle_pending_signals(process_t *proc) {
    if (!proc || proc->pending_signals == 0) {
        return;
    }

    if (proc->pending_signals & signal_bit(SIGKILL)) {
        proc->pending_signals &= ~signal_bit(SIGKILL);
        process_terminate(proc, SIGKILL);
        if (proc == process_current()) {
            process_yield();
        }
        return;
    }

    if (proc->pending_signals & signal_bit(SIGINT)) {
        proc->pending_signals &= ~signal_bit(SIGINT);
        process_terminate(proc, SIGINT);
        if (proc == process_current()) {
            process_yield();
        }
        return;
    }

    if (proc->pending_signals & signal_bit(SIGSEGV)) {
        proc->pending_signals &= ~signal_bit(SIGSEGV);
        process_terminate(proc, SIGSEGV);
        if (proc == process_current()) {
            process_yield();
        }
        return;
    }
}

i32 process_waitpid(i32 pid) {
    process_t *current = process_current();
    if (!current) {
        return -1;
    }

    current->waiting_for_pid = pid;
    while (1) {
        process_t *child = NULL;
        if (pid == -1) {
            child = process_find_any_child(current->pid);
        } else {
            child = process_find_child(current->pid, pid);
        }

        if (!child) {
            current->waiting_for_pid = -1;
            return -1;
        }

        if (child->state == PROCESS_ZOMBIE) {
            u32 child_pid = child->pid;
            current->waiting_for_pid = -1;
            process_reap(child);
            return (i32)child_pid;
        }

        current->state = PROCESS_WAITING;
        process_yield();
    }
}
