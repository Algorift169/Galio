/* kernel.c
 * Utility functions for the kernel: memset, memcpy, panic, kernel_status
 */
#include "common.h"
#include "vga.h"
#include "kprintf.h"
#include "pit.h"
#include "process.h"

void *memset(void *s, int c, u32 n) {
    u8 *p = (u8*)s;
    while (n--) *p++ = (u8)c;
    return s;
}

void *memcpy(void *dest, const void *src, u32 n) {
    u8 *d = (u8*)dest;
    const u8 *s = (const u8*)src;
    while (n--) *d++ = *s++;
    return dest;
}

void panic(const char *msg) {
    kprintf("KERNEL PANIC: %s\n", msg);
    for (;;) __asm__ volatile("cli; hlt");
}

/* Show ongoing kernel status */
void kernel_status(void) {
    u32 ticks = pit_get_ticks();
    u32 pid   = process_current() ? process_current()->pid : 0;
    kprintf("[kernel] PID=%u, uptime=%u ticks\n", pid, ticks);
}