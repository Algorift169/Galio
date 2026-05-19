#ifndef PAGING_H
#define PAGING_H

#define PAGE_PRESENT      0x001
#define PAGE_RW           0x002
#define PAGE_USER         0x004
#define PAGE_WRITETHROUGH 0x008
#define PAGE_NOCACHE      0x010
#define PAGE_ACCESSED     0x020
#define PAGE_DIRTY        0x040

#include "common.h"
#include "cpu.h"

typedef struct {
    u32 *directory;
    u32 *tables[1024];
} page_directory_t;

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

#endif /* PAGING_H */
