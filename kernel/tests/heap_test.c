/* heap_test.c - Kernel heap tests */
#include "kprintf.h"
#include "heap.h"
#include "string.h"

void heap_test(void) {
    kprintf("[KTEST] heap_test\n");

    void *ptr = kmalloc(16);
    if (!ptr) {
        kprintf("[KTEST FAIL] heap_test: kmalloc failed\n");
        return;
    }
    memset(ptr, 0xAA, 16);
    kfree(ptr);

    void *calloc_ptr = kcalloc(4, 4);
    if (!calloc_ptr) {
        kprintf("[KTEST FAIL] heap_test: kcalloc failed\n");
        return;
    }
    for (u32 i = 0; i < 16; i++) {
        if (((u8 *)calloc_ptr)[i] != 0) {
            kprintf("[KTEST FAIL] heap_test: kcalloc did not zero memory\n");
            kfree(calloc_ptr);
            return;
        }
    }

    void *realloc_ptr = krealloc(calloc_ptr, 64);
    if (!realloc_ptr) {
        kprintf("[KTEST FAIL] heap_test: krealloc failed\n");
        return;
    }

    kfree(realloc_ptr);

    void *dma_ptr = dma_alloc(4096);
    if (!dma_ptr) {
        kprintf("[KTEST FAIL] heap_test: dma_alloc failed\n");
        return;
    }
    dma_free(dma_ptr, 4096);

    kprintf("[KTEST] heap_test passed\n");
}
