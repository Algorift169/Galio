#ifndef PAGING_H
#define PAGING_H

#include "common.h"
#include "arch/x86/cpu.h"
#include "process/process.h"

#define PAGE_SIZE             4096
#define PAGE_ENTRIES          1024
#define PAGE_DIRECTORY_SIZE   0x400000

#define PAGE_PRESENT          0x001
#define PAGE_RW               0x002
#define PAGE_USER             0x004
#define PAGE_WRITETHROUGH     0x008
#define PAGE_NOCACHE          0x010
#define PAGE_ACCESSSED        0x020
#define PAGE_DIRTY            0x040

#define KERNEL_BASE           0xC0000000u
#define KERNEL_SPACE_START    (KERNEL_BASE)
#define KERNEL_SPACE_END      0xFFFFFFFFu
#define USER_SPACE_START      0x00000000u
#define USER_SPACE_END        0xBFFFFFFFu

#define PAGE_ALIGN_DOWN(x)    ((x) & 0xFFFFF000u)
#define PAGE_ALIGN_UP(x)      (((x) + PAGE_SIZE - 1) & 0xFFFFF000u)

typedef struct {
    u32 *directory;
    u32 *tables[PAGE_ENTRIES];
} page_directory_t;

typedef enum {
    PAGE_FAULT_UNHANDLED = 0,
    PAGE_FAULT_HANDLED = 1
} paging_fault_result_t;

void paging_init(void);
page_directory_t *paging_create_directory(void);
page_directory_t *paging_create_user_directory(void);
void paging_map(page_directory_t *pd, u32 vaddr, u32 paddr, u32 flags);
void paging_unmap(page_directory_t *pd, u32 vaddr);
u32 paging_get_physical(page_directory_t *pd, u32 vaddr);
void paging_enable(page_directory_t *pd);
page_directory_t *paging_get_current(void);
page_directory_t *paging_clone_directory(page_directory_t *src);
void paging_load_directory(page_directory_t *pd);
paging_fault_result_t paging_handle_page_fault(registers_t *regs);

#endif /* PAGING_H */
