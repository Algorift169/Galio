/* paging_test.c - Kernel paging tests */
#include "kprintf.h"
#include "paging.h"
#include "pmem.h"

void paging_test(void) {
    kprintf("[KTEST] paging_test\n");

    page_directory_t *pd = paging_create_directory();
    if (!pd) {
        kprintf("[KTEST FAIL] paging_test: paging_create_directory failed\n");
        return;
    }

    u32 phys = pmem_alloc(1);
    if (!phys) {
        kprintf("[KTEST FAIL] paging_test: pmem_alloc failed\n");
        pmem_free((u32)pd->directory, 1);
        return;
    }

    const u32 vaddr = 0x400000;
    paging_map(pd, vaddr, phys, PAGE_PRESENT | PAGE_RW | PAGE_USER);

    u32 resolved = paging_get_physical(pd, vaddr);
    if (resolved != phys) {
        kprintf("[KTEST FAIL] paging_test: expected phys=%08X got=%08X\n", phys, resolved);
    } else {
        kprintf("[KTEST] paging_test map/resolve passed\n");
    }

    u32 pd_idx = (vaddr >> 22) & 0x3FF;
    u32 pt_idx = (vaddr >> 12) & 0x3FF;
    volatile u32 *pt = (volatile u32 *)pd->tables[pd_idx];
    if (!pt) {
        kprintf("[KTEST FAIL] paging_test: page table was not allocated\n");
    } else if ((pt[pt_idx] & 0xFFFFF000) != phys) {
        kprintf("[KTEST FAIL] paging_test: page table entry mismatch\n");
    }

    paging_unmap(pd, vaddr);
    if (paging_get_physical(pd, vaddr) != 0) {
        kprintf("[KTEST FAIL] paging_test: paging_unmap did not clear mapping\n");
    } else {
        kprintf("[KTEST] paging_test unmap passed\n");
    }

    if (pd->tables[pd_idx]) {
        pmem_free((u32)pd->tables[pd_idx], 1);
        pd->tables[pd_idx] = NULL;
        pd->directory[pd_idx] = 0;
    }
    pmem_free((u32)pd->directory, 1);
    pmem_free(phys, 1);
}
