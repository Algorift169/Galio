/* paging.c - Virtual memory paging */
#include "paging.h"
#include "pmem.h"
#include "kprintf.h"
#include "cpu.h"
#include "process/process.h"

#define MAX_PAGE_DIRS 64
#define KERNEL_IDENTITY_MAP_SIZE 0x01000000u
#define KERNEL_PDE_START (KERNEL_BASE >> 22)

static page_directory_t page_directory_pool[MAX_PAGE_DIRS];
static u32 page_directory_count = 0;
static page_directory_t *kernel_pd = NULL;
static page_directory_t *current_page_directory = NULL;

static inline u32 get_page_directory_index(uintptr_t vaddr) {
    return (u32)((vaddr >> 22) & 0x3FFu);
}

static inline u32 get_page_table_index(uintptr_t vaddr) {
    return (u32)((vaddr >> 12) & 0x3FFu);
}

static inline void *phys_to_virt(uintptr_t phys) {
    return (void *)(uintptr_t)phys;
}

static page_directory_t *alloc_page_directory(void) {
    if (page_directory_count >= MAX_PAGE_DIRS) return NULL;
    page_directory_t *pd = &page_directory_pool[page_directory_count++];
    pd->directory = NULL;
    for (u32 i = 0; i < PAGE_ENTRIES; i++) pd->tables[i] = NULL;
    return pd;
}

static u32 alloc_page_table(void) {
    u32 pt_phys = pmem_alloc_region(1, 0x100000, 0x1000000);
    if (!pt_phys) {
        kprintf("paging: low-memory page table allocation failed\n");
        return 0;
    }

    volatile u32 *pt = (volatile u32 *)phys_to_virt(pt_phys);
    for (u32 i = 0; i < PAGE_ENTRIES; i++) pt[i] = 0;
    return pt_phys;
}

static uintptr_t *get_page_table(page_directory_t *pd, uintptr_t vaddr, u32 directory_flags, u8 create) {
    u32 pd_idx = get_page_directory_index(vaddr);
    uintptr_t *table = pd->tables[pd_idx];
    if (table) return table;
    if (!create) return NULL;

    uintptr_t pt_phys = alloc_page_table();
    if (!pt_phys) return NULL;

    pd->tables[pd_idx] = phys_to_virt(pt_phys);
    pd->directory[pd_idx] = (pt_phys & 0xFFFFF000ul) | (directory_flags & (PAGE_PRESENT | PAGE_RW | PAGE_USER | PAGE_WRITETHROUGH | PAGE_NOCACHE));
    return pd->tables[pd_idx];
}

static uintptr_t *get_page_entry(page_directory_t *pd, uintptr_t vaddr) {
    u32 pd_idx = get_page_directory_index(vaddr);
    u32 pt_idx = get_page_table_index(vaddr);
    if (!pd->tables[pd_idx]) return NULL;
    return &pd->tables[pd_idx][pt_idx];
}

static u8 is_user_address(uintptr_t addr) {
    return addr <= (uintptr_t)USER_SPACE_END;
}

static u8 is_user_stack_address(uintptr_t addr) {
    return addr >= (uintptr_t)(USER_STACK_TOP - USER_STACK_SIZE + PAGE_SIZE) && addr < (uintptr_t)USER_STACK_TOP;
}

static u8 is_user_heap_address(uintptr_t addr) {
    return addr >= (uintptr_t)USER_HEAP_START && addr < (uintptr_t)USER_HEAP_END;
}

static u8 allocate_user_page(page_directory_t *pd, uintptr_t vaddr) {
    uintptr_t phys = pmem_alloc(1);
    if (!phys) {
        kprintf("paging: unable to allocate user page for 0x%016llX\n", (unsigned long long)vaddr);
        return 0;
    }

    uintptr_t *table = get_page_table(pd, vaddr, PAGE_PRESENT | PAGE_RW | PAGE_USER, true);
    if (!table) {
        pmem_free(phys, 1);
        kprintf("paging: out of page table space for 0x%016llX\n", (unsigned long long)vaddr);
        return 0;
    }

    table[get_page_table_index(vaddr)] = (phys & 0xFFFFF000ul) | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    u8 *page = (u8 *)PAGE_ALIGN_DOWN(vaddr);
    for (u32 i = 0; i < PAGE_SIZE; i++) page[i] = 0;
    __asm__ volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
    return 1;
}

static void handle_cow_fault(page_directory_t *pd, uintptr_t fault_addr) {
    uintptr_t page_base = PAGE_ALIGN_DOWN(fault_addr);
    uintptr_t *pte = get_page_entry(pd, page_base);
    if (!pte) return;

    uintptr_t pte_val = *pte;
    if (!(pte_val & PAGE_PRESENT) || !(pte_val & PAGE_USER) || (pte_val & PAGE_RW)) return;

    uintptr_t phys = pte_val & 0xFFFFF000ul;
    if (pmem_get_refcount((u32)phys) <= 1) {
        *pte |= PAGE_RW;
        __asm__ volatile("invlpg (%0)" : : "r"(page_base) : "memory");
        return;
    }

    uintptr_t new_phys = pmem_alloc(1);
    if (!new_phys) {
        kprintf("paging: COW failed, unable to allocate new page for 0x%016llX\n", (unsigned long long)page_base);
        return;
    }

    u8 *src = (u8 *)page_base;
    u8 *dst = (u8 *)(uintptr_t)new_phys;
    for (u32 i = 0; i < PAGE_SIZE; i++) dst[i] = src[i];

    pmem_refcount_dec((u32)phys);
    *pte = (new_phys & 0xFFFFF000ul) | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    __asm__ volatile("invlpg (%0)" : : "r"(page_base) : "memory");
}

static void map_kernel_region(page_directory_t *pd, uintptr_t vaddr, uintptr_t paddr, u32 flags) {
    u32 pd_idx = get_page_directory_index(vaddr);
    u32 pt_idx = get_page_table_index(vaddr);
    uintptr_t *table = pd->tables[pd_idx];
    if (!table) {
        uintptr_t pt_phys = alloc_page_table();
        if (!pt_phys) panic("paging: failed to allocate kernel page table");
        pd->tables[pd_idx] = phys_to_virt(pt_phys);
        pd->directory[pd_idx] = (pt_phys & 0xFFFFF000ul) | (flags & (PAGE_PRESENT | PAGE_RW | PAGE_USER));
        table = pd->tables[pd_idx];
    }
    table[pt_idx] = (paddr & 0xFFFFF000ul) | (flags & (PAGE_PRESENT | PAGE_RW | PAGE_USER | PAGE_WRITETHROUGH | PAGE_NOCACHE));
}

void paging_init(void) {
    kprintf("Initializing paging system (64-bit Long Mode)...\n");

    kernel_pd = alloc_page_directory();
    if (!kernel_pd) return;

    uintptr_t cr3_val;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3_val));
    kernel_pd->directory = (uintptr_t *)cr3_val;
    current_page_directory = kernel_pd;

    kprintf("paging: kernel 64-bit identity paging active (CR3=0x%016llX)\n", (unsigned long long)cr3_val);
}

page_directory_t *paging_create_directory(void) {
    page_directory_t *pd = alloc_page_directory();
    if (!pd) return NULL;

    u32 pd_phys = pmem_alloc_region(1, 0x100000, 0x1000000);
    if (!pd_phys) {
        kprintf("paging: low-memory page directory allocation failed\n");
        return NULL;
    }

    volatile u32 *pd_virt = (volatile u32 *)phys_to_virt(pd_phys);
    for (u32 i = 0; i < PAGE_ENTRIES; i++) pd_virt[i] = 0;

    pd->directory = phys_to_virt(pd_phys);
    for (u32 i = 0; i < PAGE_ENTRIES; i++) pd->tables[i] = NULL;
    return pd;
}

page_directory_t *paging_create_user_directory(void) {
    page_directory_t *pd = paging_create_directory();
    if (!pd) return NULL;

    if (!kernel_pd) return pd;

    /* User page directories should not expose the low kernel mappings.
     * Keep the first page unmapped in user space to ensure NULL dereferences
     * fault in user mode even if the kernel itself still needs virtual page 0.
     */
    for (u32 i = KERNEL_PDE_START; i < PAGE_ENTRIES; i++) {
        if (kernel_pd->directory[i] & PAGE_PRESENT) {
            pd->directory[i] = kernel_pd->directory[i] & ~PAGE_USER;
            pd->tables[i] = kernel_pd->tables[i];
        }
    }

    /* Since the kernel is linked in low memory (0x100000), we must also map the
     * low identity-mapped kernel region (first 16MB) in all user directories,
     * but mark it as supervisor-only (~PAGE_USER).
     */
    for (u32 i = 0; i < (KERNEL_IDENTITY_MAP_SIZE >> 22); i++) {
        if (kernel_pd->directory[i] & PAGE_PRESENT) {
            pd->directory[i] = kernel_pd->directory[i] & ~PAGE_USER;
            pd->tables[i] = kernel_pd->tables[i];
        }
    }
    return pd;
}

void paging_map(page_directory_t *pd, uintptr_t vaddr, uintptr_t paddr, u32 flags) {
    if (!pd) return;
    if (vaddr < 0x100000000ULL) return; /* Lower 4GB is identity mapped */

    uintptr_t *table = get_page_table(pd, vaddr, PAGE_PRESENT | PAGE_RW | (flags & PAGE_USER), true);
    if (!table) {
        kprintf("paging_map: Failed to allocate page table for 0x%016llX\n", (unsigned long long)vaddr);
        return;
    }

    table[get_page_table_index(vaddr)] = (paddr & 0xFFFFF000ul) | (flags & (PAGE_PRESENT | PAGE_RW | PAGE_USER | PAGE_WRITETHROUGH | PAGE_NOCACHE));
    __asm__ volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
}

/* Map a kernel virtual address into the global kernel page directory. This
 * helper allows creating per-process kernel mappings (e.g. stacks) while
 * preserving the rest of kernel mappings. Uses the internal `kernel_pd`.
 */
void paging_map_kernel(uintptr_t vaddr, uintptr_t paddr, u32 flags) {
    if (!kernel_pd) return;
    map_kernel_region(kernel_pd, vaddr, paddr, flags & (PAGE_PRESENT | PAGE_RW | PAGE_USER | PAGE_WRITETHROUGH | PAGE_NOCACHE));
    __asm__ volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
}

/* Unmap a kernel virtual address from the global kernel page directory. */
void paging_unmap_kernel(uintptr_t vaddr) {
    if (!kernel_pd) return;
    u32 pd_idx = get_page_directory_index(vaddr);
    u32 pt_idx = get_page_table_index(vaddr);
    if (kernel_pd->tables[pd_idx]) {
        uintptr_t *table = kernel_pd->tables[pd_idx];
        table[pt_idx] = 0;
        __asm__ volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
    }
}

void paging_unmap(page_directory_t *pd, uintptr_t vaddr) {
    if (!pd) return;
    u32 pd_idx = get_page_directory_index(vaddr);
    u32 pt_idx = get_page_table_index(vaddr);
    if (pd->tables[pd_idx]) {
        pd->tables[pd_idx][pt_idx] = 0;
        __asm__ volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
    }
}

uintptr_t paging_get_physical(page_directory_t *pd, uintptr_t vaddr) {
    if (!pd) return 0;
    u32 pd_idx = get_page_directory_index(vaddr);
    u32 pt_idx = get_page_table_index(vaddr);
    if (!pd->tables[pd_idx]) return 0;
    uintptr_t pte = pd->tables[pd_idx][pt_idx];
    if (!(pte & PAGE_PRESENT)) return 0;
    return (pte & 0xFFFFF000ul) | (vaddr & 0xFFFul);
}

u8 paging_validate_user_range(page_directory_t *pd, uintptr_t vaddr, uintptr_t length, u8 write) {
    if (!pd || length == 0) {
        return length == 0;
    }

    if (!is_user_address(vaddr)) {
        return 0;
    }

    uintptr_t end = vaddr + length - 1;
    if (end < vaddr || !is_user_address(end)) {
        return 0;
    }

    for (uintptr_t page = PAGE_ALIGN_DOWN(vaddr); page <= PAGE_ALIGN_DOWN(end); page += PAGE_SIZE) {
        uintptr_t *pte = get_page_entry(pd, page);
        if (!pte || !(*pte & PAGE_PRESENT) || !(*pte & PAGE_USER)) {
            return 0;
        }
        if (write && !(*pte & PAGE_RW)) {
            return 0;
        }
        if (page == PAGE_ALIGN_DOWN(end)) {
            break;
        }
    }

    return 1;
}

void paging_enable(page_directory_t *pd) {
    if (!pd || !pd->directory) return;
    uintptr_t pd_phys = (uintptr_t)pd->directory;
    __asm__ volatile("mov %0, %%cr3" : : "r"(pd_phys));
    current_page_directory = pd;
}

page_directory_t *paging_get_current(void) {
    return current_page_directory ? current_page_directory : kernel_pd;
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

        uintptr_t *old_pt = src->tables[pd_idx];
        uintptr_t *new_pt = phys_to_virt(new_pt_phys);

        for (u32 pt_idx = 0; pt_idx < PAGE_ENTRIES; pt_idx++) {
            uintptr_t pte = old_pt[pt_idx];
            if (!(pte & PAGE_PRESENT)) {
                new_pt[pt_idx] = 0;
                continue;
            }

            if (pte & PAGE_USER) {
                if (pte & PAGE_RW) {
                    pte &= ~PAGE_RW;
                    old_pt[pt_idx] &= ~PAGE_RW;
                }
                pmem_refcount_inc(pte & 0xFFFFF000u);
            }

            new_pt[pt_idx] = pte;
        }

        clone->tables[pd_idx] = (uintptr_t *)new_pt_phys;
        clone->directory[pd_idx] = (new_pt_phys & 0xFFFFF000ul) | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    }

    return clone;
}

paging_fault_result_t paging_handle_page_fault(registers_t *regs) {
    uintptr_t fault_addr;
    __asm__ volatile("mov %%cr2, %0" : "=r"(fault_addr));

    u8 present = (regs->error_code & 0x1) ? 1 : 0;
    u8 write = (regs->error_code & 0x2) ? 1 : 0;
    u8 user = (regs->error_code & 0x4) ? 1 : 0;

    if (!user) {
        return PAGE_FAULT_UNHANDLED;
    }

    if (!present && write) {
        if (is_user_stack_address(fault_addr) || is_user_heap_address(fault_addr)) {
            if (allocate_user_page(paging_get_current(), fault_addr)) {
                return PAGE_FAULT_HANDLED;
            }
        }
    }

    if (present && write) {
        uintptr_t *pte = get_page_entry(paging_get_current(), PAGE_ALIGN_DOWN(fault_addr));
        if (pte && (*pte & PAGE_PRESENT) && (*pte & PAGE_USER) && !(*pte & PAGE_RW)) {
            handle_cow_fault(paging_get_current(), fault_addr);
            return PAGE_FAULT_HANDLED;
        }
    }

    return PAGE_FAULT_UNHANDLED;
}

void paging_load_directory(page_directory_t *pd) {
    if (!pd) return;
    if (pd != kernel_pd && kernel_pd) {
        for (u32 i = KERNEL_PDE_START; i < PAGE_ENTRIES; i++) {
            if (kernel_pd->directory[i] & PAGE_PRESENT) {
                pd->directory[i] = kernel_pd->directory[i] & ~PAGE_USER;
                pd->tables[i] = kernel_pd->tables[i];
            } else {
                pd->directory[i] = 0;
                pd->tables[i] = NULL;
            }
        }
    }
    uintptr_t pd_phys = (uintptr_t)pd->directory;
    __asm__ volatile("mov %0, %%cr3" : : "r"(pd_phys));
    current_page_directory = pd;
}
