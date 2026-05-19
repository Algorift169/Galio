/* paging.c - Virtual memory paging */
#include "paging.h"
#include "pmem.h"
#include "kprintf.h"
#include "cpu.h"
#include "process.h"

#define PAGE_SIZE 4096
#define PAGE_ENTRIES 1024
#define MAX_PAGE_DIRS 64

static page_directory_t page_directory_pool[MAX_PAGE_DIRS];
static u32 page_directory_count = 0;
static page_directory_t *kernel_pd = NULL;

static page_directory_t *alloc_page_directory(void) {
    if (page_directory_count >= MAX_PAGE_DIRS) return NULL;
    page_directory_t *pd = &page_directory_pool[page_directory_count++];
    pd->directory = NULL;
    for (u32 i = 0; i < PAGE_ENTRIES; i++) pd->tables[i] = NULL;
    return pd;
}

static u32 alloc_page_table(void) {
    u32 pt_phys = pmem_alloc(1);
    if (!pt_phys) return 0;
    volatile u32 *pt_virt = (volatile u32 *)pt_phys;
    for (u32 i = 0; i < PAGE_ENTRIES; i++) pt_virt[i] = 0;
    return pt_phys;
}

static u32 *get_page_table(page_directory_t *pd, u32 vaddr) {
    u32 pd_idx = (vaddr >> 22) & 0x3FF;
    return pd->tables[pd_idx];
}

static u32 *get_page_entry(page_directory_t *pd, u32 vaddr) {
    u32 pd_idx = (vaddr >> 22) & 0x3FF;
    u32 pt_idx = (vaddr >> 12) & 0x3FF;
    if (!pd->tables[pd_idx]) return NULL;
    return &pd->tables[pd_idx][pt_idx];
}

static void handle_cow_fault(page_directory_t *pd, u32 fault_addr) {
    u32 page_base = fault_addr & 0xFFFFF000;
    u32 *pte = get_page_entry(pd, page_base);
    if (!pte) return;

    u32 pte_val = *pte;
    if (!(pte_val & PAGE_PRESENT) || !(pte_val & PAGE_USER)) return;

    u32 phys = pte_val & 0xFFFFF000;
    if (pmem_get_refcount(phys) <= 1) {
        *pte |= PAGE_RW;
        __asm__ volatile("invlpg (%0)" : : "r"(page_base) : "memory");
        return;
    }

    u32 new_phys = pmem_alloc(1);
    if (!new_phys) {
        kprintf("COW fault: Unable to allocate new page for 0x%08X\n", page_base);
        return;
    }

    u8 *src = (u8 *)phys;
    u8 *dst = (u8 *)new_phys;
    for (u32 i = 0; i < PAGE_SIZE; i++) dst[i] = src[i];

    pmem_refcount_dec(phys);
    *pte = (new_phys & 0xFFFFF000) | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    __asm__ volatile("invlpg (%0)" : : "r"(page_base) : "memory");
}

void paging_init(void) {
    kprintf("Initializing paging system...\n");
    kernel_pd = paging_create_directory();
    if (!kernel_pd) panic("Paging directory creation failed");

    /* Identity map first 16 MB (0x0 - 0x1000000) for now */
    kprintf("Identity-mapping first 16 MB...\n");
    for (u32 base = 0; base < 0x1000000; base += 0x400000) {
        u32 pt_phys = alloc_page_table();
        if (!pt_phys) panic("Failed to allocate page table");
        volatile u32 *pt = (volatile u32 *)pt_phys;
        for (u32 i = 0; i < 1024; i++) {
            u32 paddr = base + i * PAGE_SIZE;
            pt[i] = paddr | PAGE_PRESENT | PAGE_RW;
        }
        u32 pde_index = base >> 22;
        kernel_pd->directory[pde_index] = (pt_phys & 0xFFFFF000) | PAGE_PRESENT | PAGE_RW;
        kernel_pd->tables[pde_index] = (u32 *)pt_phys;
    }

    kprintf("Paging: mapped first 16 MB\n");
    paging_enable(kernel_pd);
    kprintf("Paging enabled successfully\n");
}

page_directory_t *paging_create_directory(void) {
    page_directory_t *pd = alloc_page_directory();
    if (!pd) return NULL;

    u32 pd_phys = pmem_alloc(1);
    if (!pd_phys) return NULL;

    volatile u32 *pd_virt = (volatile u32 *)pd_phys;
    for (u32 i = 0; i < PAGE_ENTRIES; i++) pd_virt[i] = 0;

    pd->directory = (u32 *)pd_phys;
    for (u32 i = 0; i < PAGE_ENTRIES; i++) pd->tables[i] = NULL;
    return pd;
}

page_directory_t *paging_create_user_directory(void) {
    page_directory_t *pd = paging_create_directory();
    if (!pd) return NULL;

    if (!kernel_pd) return pd;

    for (u32 i = 0; i < PAGE_ENTRIES; i++) {
        if (kernel_pd->directory[i] & PAGE_PRESENT) {
            pd->directory[i] = kernel_pd->directory[i] & ~PAGE_USER;
            pd->tables[i] = kernel_pd->tables[i];
        }
    }

    return pd;
}

void paging_map(page_directory_t *pd, u32 vaddr, u32 paddr, u32 flags) {
    u32 pd_idx = (vaddr >> 22) & 0x3FF;
    u32 pt_idx = (vaddr >> 12) & 0x3FF;

    if (!pd->tables[pd_idx]) {
        u32 pt_phys = alloc_page_table();
        if (!pt_phys) {
            kprintf("paging_map: Failed to allocate page table\n");
            return;
        }
        pd->tables[pd_idx] = (u32 *)pt_phys;
        pd->directory[pd_idx] = (pt_phys & 0xFFFFF000) | PAGE_PRESENT | PAGE_RW;
    }

    u32 pte = (paddr & 0xFFFFF000) | flags;
    pd->tables[pd_idx][pt_idx] = pte;
    __asm__ volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
}

void paging_unmap(page_directory_t *pd, u32 vaddr) {
    u32 pd_idx = (vaddr >> 22) & 0x3FF;
    u32 pt_idx = (vaddr >> 12) & 0x3FF;
    if (pd->tables[pd_idx]) {
        pd->tables[pd_idx][pt_idx] = 0;
        __asm__ volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
    }
}

u32 paging_get_physical(page_directory_t *pd, u32 vaddr) {
    u32 pd_idx = (vaddr >> 22) & 0x3FF;
    u32 pt_idx = (vaddr >> 12) & 0x3FF;
    if (!pd->tables[pd_idx]) return 0;
    u32 pte = pd->tables[pd_idx][pt_idx];
    if (!(pte & PAGE_PRESENT)) return 0;
    return (pte & 0xFFFFF000) | (vaddr & 0xFFF);
}

void paging_enable(page_directory_t *pd) {
    u32 pd_phys = (u32)pd->directory;
    __asm__ volatile("mov %0, %%cr3" : : "r"(pd_phys));
    u32 cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));
}

page_directory_t *paging_get_current(void) {
    process_t *proc = process_current();
    if (proc && proc->pagedir) return proc->pagedir;
    return kernel_pd;
}

page_directory_t *paging_clone_directory(page_directory_t *src) {
    if (!src) return NULL;
    page_directory_t *clone = paging_create_user_directory();
    if (!clone) return NULL;

    for (u32 pd_idx = 0; pd_idx < PAGE_ENTRIES; pd_idx++) {
        if (!src->tables[pd_idx]) continue;
        u32 pde = src->directory[pd_idx];
        if (!(pde & PAGE_PRESENT) || !(pde & PAGE_USER)) continue;

        u32 new_pt_phys = alloc_page_table();
        if (!new_pt_phys) return clone;

        u32 *old_pt = src->tables[pd_idx];
        u32 *new_pt = (u32 *)new_pt_phys;

        for (u32 pt_idx = 0; pt_idx < PAGE_ENTRIES; pt_idx++) {
            u32 pte = old_pt[pt_idx];
            if (pte & PAGE_PRESENT) {
                if (pte & PAGE_USER) {
                    if (pte & PAGE_RW) {
                        pte &= ~PAGE_RW;
                        old_pt[pt_idx] &= ~PAGE_RW;
                    }
                    pmem_refcount_inc(pte & 0xFFFFF000);
                }
                new_pt[pt_idx] = pte;
            }
        }

        clone->tables[pd_idx] = (u32 *)new_pt_phys;
        clone->directory[pd_idx] = (new_pt_phys & 0xFFFFF000) | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    }

    return clone;
}

void paging_load_directory(page_directory_t *pd) {
    u32 pd_phys = (u32)pd->directory;
    __asm__ volatile("mov %0, %%cr3" : : "r"(pd_phys));
}