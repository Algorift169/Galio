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

#include "tss.h"
#include "string.h"
#include "cpu.h"
#include "process.h"
#include "paging.h"

#include "common.h"

/* Task State Segment used for ring3 -> ring0 privilege switches */
tss_entry_t tss_entry;

void tss_init(void) {
    memset(&tss_entry, 0, sizeof(tss_entry));
    tss_entry.rsp0 = 0;
    tss_entry.iomap_base = sizeof(tss_entry);
}

void tss_set_kernel_stack(u64 stack) {
    tss_entry.rsp0 = stack;
}

void tss_load(void) {
    u16 selector = TSS_SELECTOR;
    __asm__ volatile("ltr %0" :: "r"(selector));
}

void enter_userspace(uintptr_t entry_point, uintptr_t user_stack) {
    process_t *proc = process_current();
    if (proc && proc->stack) {
        tss_set_kernel_stack((uintptr_t)proc->stack + proc->stack_size - 8);
    }

    page_directory_t *pd = paging_get_current();
    if (pd) {
        paging_load_directory(pd);
    }

    __asm__ volatile(
        "mov $0x23, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "pushq $0x23\n"
        "pushq %[ustack]\n"
        "pushfq\n"
        "orq $0x200, (%%rsp)\n"
        "pushq $0x1B\n"
        "pushq %[entry]\n"
        "iretq\n"
        :
        : [ustack] "r" (user_stack),
          [entry] "r" (entry_point)
        : "rax", "memory"
    );

    for (;;);
}

