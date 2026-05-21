/* kernel.c
 * Utility functions for the kernel: memset, memcpy, panic, kernel_status
 */
#include "common.h"
#include "vga.h"
#include "kprintf.h"
#include "pit.h"
#include "process.h"
#include "serial.h"

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

void stack_trace(void) {
    u32 *ebp;
    __asm__ volatile("mov %%ebp, %0" : "=r"(ebp));

    kprintf("Stack trace:\n");
    for (u32 depth = 0; ebp && depth < 16; depth++) {
        u32 return_addr = ebp[1];
        kprintf("  #%u: EIP=0x%08X EBP=0x%08X\n", depth, return_addr, (u32)ebp);
        ebp = (u32 *)ebp[0];
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