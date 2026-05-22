/* run_tests.c - Kernel test runner (lightweight stubs) */
#include "kprintf.h"

extern void scheduler_test(void);
extern void paging_test(void);
extern void vfs_test(void);
extern void signal_test(void);
extern void heap_test(void);

void run_kernel_tests(void) {
    kprintf("[KTEST] Running kernel tests (stubs)\n");
    scheduler_test();
    paging_test();
    vfs_test();
    signal_test();
    heap_test();
    kprintf("[KTEST] Kernel tests completed\n");
}
