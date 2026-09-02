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

/* kernel.c
 * Utility functions for the kernel: memset, memcpy, panic, kernel_status
 */
#include "common.h"
#include "vga.h"
#include "kprintf.h"
#include "pit.h"
#include "process.h"
#include "serial.h"

void *memset(void *s, int c, size_t n) {
    u8 *p = (u8*)s;
    while (n--) *p++ = (u8)c;
    return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
    u8 *d = (u8*)dest;
    const u8 *s = (const u8*)src;
    while (n--) *d++ = *s++;
    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    u8 *d = (u8*)dest;
    const u8 *s = (const u8*)src;
    if (d == s || n == 0) return dest;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        /* copy backwards to handle overlap */
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dest;
}

void stack_trace(void) {
    uintptr_t *rbp;
    __asm__ volatile("mov %%rbp, %0" : "=r"(rbp));

    kprintf("Stack trace:\n");
    for (u32 depth = 0; rbp && depth < 16; depth++) {
        uintptr_t return_addr = rbp[1];
        kprintf("  #%u: RIP=0x%016llX RBP=0x%016llX\n",
                depth,
                (unsigned long long)return_addr,
                (unsigned long long)(uintptr_t)rbp);
        rbp = (uintptr_t *)rbp[0];
    }
}

void assert_failed(const char *expr, const char *file, i32 line) {
    kprintf("ASSERTION FAILED: %s\n", expr);
    kprintf("  at %s:%d\n", file, line);
    stack_trace();
    panic("Assertion failed");
}

void panic(const char *msg) {
    kprintf("KERNEL PANIC: %s\n", msg);
    serial_puts("KERNEL PANIC\n");
    stack_trace();
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

/* Show ongoing kernel status */
void kernel_status(void) {
    u32 ticks = pit_get_ticks();
    u32 pid   = process_current() ? process_current()->pid : 0;
    kprintf("[kernel] PID=%u, uptime=%u ticks\n", pid, ticks);
}