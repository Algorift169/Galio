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