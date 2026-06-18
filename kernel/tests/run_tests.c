/* run_tests.c - Kernel test runner */
#include "kprintf.h"

extern void scheduler_test(void);
extern void cpu_scheduler_test(void);
extern void paging_test(void);
extern void vfs_test(void);
extern void signal_test(void);
extern void heap_test(void);
extern void security_test(void);

void run_kernel_tests(void) {
    kprintf("\n========================================\n");
    kprintf("     KERNEL SELF-TEST SUITE\n");
    kprintf("========================================\n");

    scheduler_test();
    cpu_scheduler_test();
    paging_test();
    vfs_test();
    signal_test();
    heap_test();
    security_test();

    kprintf("========================================\n");
    kprintf("[KTEST] Kernel self-tests completed\n");
    kprintf("========================================\n\n");
}
