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

/* test_elf.c - Minimal ELF test binary that writes to VGA via syscall */

#define SYS_WRITE 2
#define SYS_EXIT  1

typedef unsigned int u32;

static inline int syscall3(int num, int arg1, int arg2, int arg3) {
    int ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3)
        : "memory"
    );
    return ret;
}

void _start(void) {
    const char msg[] = "Hello from ELF user task!\n";

    for (int i = 0; i < 8; i++) {
        syscall3(SYS_WRITE, 1, (int)msg, sizeof(msg) - 1);
        for (volatile u32 delay = 0; delay < 2000000; delay++) {
            /* Busy loop to force timer preemption */
        }
    }

    /* Exit */
    syscall3(SYS_EXIT, 0, 0, 0);
}