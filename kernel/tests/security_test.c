/* security_test.c - Kernel security hardening tests */
#include "kprintf.h"
#include "paging.h"
#include "pmem.h"

void security_test(void) {
    kprintf("[KTEST] security_test\n");

    page_directory_t *pd = paging_create_directory();
    if (!pd) {
        kprintf("[KTEST FAIL] security_test: paging_create_directory failed\n");
        return;
    }

    u32 phys = pmem_alloc(1);
    if (!phys) {
        kprintf("[KTEST FAIL] security_test: pmem_alloc failed\n");
        pmem_free((u32)pd->directory, 1);
        return;
    }

    const u32 user_page = 0x40000000u;
    paging_map(pd, user_page, phys, PAGE_PRESENT | PAGE_USER);

    if (!paging_validate_user_range(pd, user_page, 16, 0)) {
        kprintf("[KTEST FAIL] security_test: readable user page rejected\n");
    }
    if (paging_validate_user_range(pd, user_page, 16, 1)) {
        kprintf("[KTEST FAIL] security_test: read-only user page accepted for write\n");
    }
    if (paging_validate_user_range(pd, KERNEL_SPACE_START, 16, 0)) {
        kprintf("[KTEST FAIL] security_test: kernel address accepted as user range\n");
    }
    if (paging_validate_user_range(pd, 0xFFFFF000u, 0x2000u, 0)) {
        kprintf("[KTEST FAIL] security_test: overflowing user range accepted\n");
    }
    if (paging_validate_user_range(pd, user_page + PAGE_SIZE, 16, 0)) {
        kprintf("[KTEST FAIL] security_test: unmapped user page accepted\n");
    }

    paging_unmap(pd, user_page);
    if (pd->tables[(user_page >> 22) & 0x3FFu]) {
        pmem_free((u32)pd->tables[(user_page >> 22) & 0x3FFu], 1);
    }
    pmem_free((u32)pd->directory, 1);
    pmem_free(phys, 1);
    kprintf("[KTEST] security_test completed\n");
}
