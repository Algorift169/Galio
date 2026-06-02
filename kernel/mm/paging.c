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

static inline u32 get_page_directory_index(u32 vaddr) {
    return (vaddr >> 22) & 0x3FFu;
}

static inline u32 get_page_table_index(u32 vaddr) {
    return (vaddr >> 12) & 0x3FFu;
}

static inline u32 *phys_to_virt(u32 phys) {
    return (u32 *)(uintptr_t)phys;
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

static u32 *get_page_table(page_directory_t *pd, u32 vaddr, u32 directory_flags, u8 create) {
    u32 pd_idx = get_page_directory_index(vaddr);
    u32 *table = pd->tables[pd_idx];
    if (table) return table;
    if (!create) return NULL;

    u32 pt_phys = alloc_page_table();
    if (!pt_phys) return NULL;

    pd->tables[pd_idx] = phys_to_virt(pt_phys);
    pd->directory[pd_idx] = (pt_phys & 0xFFFFF000u) | (directory_flags & (PAGE_PRESENT | PAGE_RW | PAGE_USER | PAGE_WRITETHROUGH | PAGE_NOCACHE));
    return pd->tables[pd_idx];
}

static u32 *get_page_entry(page_directory_t *pd, u32 vaddr) {
    u32 pd_idx = get_page_directory_index(vaddr);
    u32 pt_idx = get_page_table_index(vaddr);
    if (!pd->tables[pd_idx]) return NULL;
    return &pd->tables[pd_idx][pt_idx];
}

static u8 is_user_address(u32 addr) {
    return addr <= USER_SPACE_END;
}

static u8 is_user_stack_address(u32 addr) {
    return addr >= (USER_STACK_TOP - USER_STACK_SIZE + PAGE_SIZE) && addr < USER_STACK_TOP;
}

static u8 is_user_heap_address(u32 addr) {
    return addr >= USER_HEAP_START && addr < USER_HEAP_END;
}

static u8 allocate_user_page(page_directory_t *pd, u32 vaddr) {
    u32 phys = pmem_alloc(1);
    if (!phys) {
        kprintf("paging: unable to allocate user page for 0x%08X\n", vaddr);
        return 0;
    }

    u32 *table = get_page_table(pd, vaddr, PAGE_PRESENT | PAGE_RW | PAGE_USER, true);
    if (!table) {
        pmem_free(phys, 1);
        kprintf("paging: out of page table space for 0x%08X\n", vaddr);
        return 0;
    }

    table[get_page_table_index(vaddr)] = (phys & 0xFFFFF000u) | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    u8 *page = (u8 *)PAGE_ALIGN_DOWN(vaddr);
    for (u32 i = 0; i < PAGE_SIZE; i++) page[i] = 0;
    __asm__ volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
    return 1;
}

static void handle_cow_fault(page_directory_t *pd, u32 fault_addr) {
    u32 page_base = PAGE_ALIGN_DOWN(fault_addr);
    u32 *pte = get_page_entry(pd, page_base);
    if (!pte) return;

    u32 pte_val = *pte;
    if (!(pte_val & PAGE_PRESENT) || !(pte_val & PAGE_USER) || (pte_val & PAGE_RW)) return;

    u32 phys = pte_val & 0xFFFFF000u;
    if (pmem_get_refcount(phys) <= 1) {
        *pte |= PAGE_RW;
        __asm__ volatile("invlpg (%0)" : : "r"(page_base) : "memory");
        return;
    }

    u32 new_phys = pmem_alloc(1);
    if (!new_phys) {
        kprintf("paging: COW failed, unable to allocate new page for 0x%08X\n", page_base);
        return;
    }

    u8 *src = (u8 *)page_base;
    u8 *dst = (u8 *)new_phys;
    for (u32 i = 0; i < PAGE_SIZE; i++) dst[i] = src[i];

    pmem_refcount_dec(phys);
    *pte = (new_phys & 0xFFFFF000u) | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    __asm__ volatile("invlpg (%0)" : : "r"(page_base) : "memory");
}

static void map_kernel_region(page_directory_t *pd, u32 vaddr, u32 paddr, u32 flags) {
    u32 pd_idx = get_page_directory_index(vaddr);
    u32 pt_idx = get_page_table_index(vaddr);
    u32 *table = pd->tables[pd_idx];
    if (!table) {
        u32 pt_phys = alloc_page_table();
        if (!pt_phys) panic("paging: failed to allocate kernel page table");
        pd->tables[pd_idx] = phys_to_virt(pt_phys);
        pd->directory[pd_idx] = (pt_phys & 0xFFFFF000u) | (flags & (PAGE_PRESENT | PAGE_RW | PAGE_USER));
        table = pd->tables[pd_idx];
    }
    table[pt_idx] = (paddr & 0xFFFFF000u) | (flags & (PAGE_PRESENT | PAGE_RW | PAGE_USER | PAGE_WRITETHROUGH | PAGE_NOCACHE));
}

void paging_init(void) {
    kprintf("Initializing paging system...\n");

    kernel_pd = paging_create_directory();
    if (!kernel_pd) panic("paging: directory creation failed");

    kprintf("paging: creating kernel identity and higher-half mappings\n");
    for (u32 pde = 0; pde < (KERNEL_IDENTITY_MAP_SIZE >> 22); pde++) {
        u32 pt_phys = alloc_page_table();
        if (!pt_phys) panic("paging: failed to allocate kernel page table");

        u32 *pt = phys_to_virt(pt_phys);
        u32 base = pde << 22;
        for (u32 idx = 0; idx < PAGE_ENTRIES; idx++) {
            u32 phys = base + (idx << 12);
            pt[idx] = phys | PAGE_PRESENT | PAGE_RW;
        }

        kernel_pd->tables[pde] = (u32 *)pt_phys;
        kernel_pd->directory[pde] = (pt_phys & 0xFFFFF000u) | PAGE_PRESENT | PAGE_RW;

        u32 mirror_index = pde + KERNEL_PDE_START;
        kernel_pd->tables[mirror_index] = (u32 *)pt_phys;
        kernel_pd->directory[mirror_index] = (pt_phys & 0xFFFFF000u) | PAGE_PRESENT | PAGE_RW;
    }

    paging_enable(kernel_pd);
    kprintf("paging: kernel paging enabled, higher-half base=0x%08X\n", KERNEL_BASE);
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
    return pd;
}

void paging_map(page_directory_t *pd, u32 vaddr, u32 paddr, u32 flags) {
    if (!pd) return;

    u32 *table = get_page_table(pd, vaddr, PAGE_PRESENT | PAGE_RW | (flags & PAGE_USER), true);
    if (!table) {
        kprintf("paging_map: Failed to allocate page table for 0x%08X\n", vaddr);
        return;
    }

    table[get_page_table_index(vaddr)] = (paddr & 0xFFFFF000u) | (flags & (PAGE_PRESENT | PAGE_RW | PAGE_USER | PAGE_WRITETHROUGH | PAGE_NOCACHE));
    __asm__ volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
}

/* Map a kernel virtual address into the global kernel page directory. This
 * helper allows creating per-process kernel mappings (e.g. stacks) while
 * preserving the rest of kernel mappings. Uses the internal `kernel_pd`.
 */
void paging_map_kernel(u32 vaddr, u32 paddr, u32 flags) {
    if (!kernel_pd) return;
    map_kernel_region(kernel_pd, vaddr, paddr, flags & (PAGE_PRESENT | PAGE_RW | PAGE_USER | PAGE_WRITETHROUGH | PAGE_NOCACHE));
    __asm__ volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
}

/* Unmap a kernel virtual address from the global kernel page directory. */
void paging_unmap_kernel(u32 vaddr) {
    if (!kernel_pd) return;
    u32 pd_idx = get_page_directory_index(vaddr);
    u32 pt_idx = get_page_table_index(vaddr);
    if (kernel_pd->tables[pd_idx]) {
        u32 *table = kernel_pd->tables[pd_idx];
        table[pt_idx] = 0;
        __asm__ volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
    }
}

void paging_unmap(page_directory_t *pd, u32 vaddr) {
    if (!pd) return;
    u32 pd_idx = get_page_directory_index(vaddr);
    u32 pt_idx = get_page_table_index(vaddr);
    if (pd->tables[pd_idx]) {
        pd->tables[pd_idx][pt_idx] = 0;
        __asm__ volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
    }
}

u32 paging_get_physical(page_directory_t *pd, u32 vaddr) {
    if (!pd) return 0;
    u32 pd_idx = get_page_directory_index(vaddr);
    u32 pt_idx = get_page_table_index(vaddr);
    if (!pd->tables[pd_idx]) return 0;
    u32 pte = pd->tables[pd_idx][pt_idx];
    if (!(pte & PAGE_PRESENT)) return 0;
    return (pte & 0xFFFFF000u) | (vaddr & 0xFFFu);
}

u8 paging_validate_user_range(page_directory_t *pd, u32 vaddr, u32 length, u8 write) {
    if (!pd || length == 0) {
        return length == 0;
    }

    if (!is_user_address(vaddr)) {
        return 0;
    }

    u32 end = vaddr + length - 1;
    if (end < vaddr || !is_user_address(end)) {
        return 0;
    }

    for (u32 page = PAGE_ALIGN_DOWN(vaddr); page <= PAGE_ALIGN_DOWN(end); page += PAGE_SIZE) {
        u32 *pte = get_page_entry(pd, page);
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
    if (!pd) return;
    u32 pd_phys = (u32)pd->directory;
    __asm__ volatile("mov %0, %%cr3" : : "r"(pd_phys));

    u32 cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u;
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));

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

        u32 *old_pt = src->tables[pd_idx];
        u32 *new_pt = phys_to_virt(new_pt_phys);

        for (u32 pt_idx = 0; pt_idx < PAGE_ENTRIES; pt_idx++) {
            u32 pte = old_pt[pt_idx];
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

        clone->tables[pd_idx] = (u32 *)new_pt_phys;
        clone->directory[pd_idx] = (new_pt_phys & 0xFFFFF000u) | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    }

    return clone;
}

paging_fault_result_t paging_handle_page_fault(registers_t *regs) {
    u32 fault_addr;
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
        u32 *pte = get_page_entry(paging_get_current(), PAGE_ALIGN_DOWN(fault_addr));
        if (pte && (*pte & PAGE_PRESENT) && (*pte & PAGE_USER) && !(*pte & PAGE_RW)) {
            handle_cow_fault(paging_get_current(), fault_addr);
            return PAGE_FAULT_HANDLED;
        }
    }

    return PAGE_FAULT_UNHANDLED;
}

void paging_load_directory(page_directory_t *pd) {
    if (!pd) return;
    u32 pd_phys = (u32)pd->directory;
    __asm__ volatile("mov %0, %%cr3" : : "r"(pd_phys));
    current_page_directory = pd;
}
