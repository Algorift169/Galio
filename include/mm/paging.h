/*
 * Galio Kernel
 *
 * Copyright (C) 2026 S.M Israfil
 *
 * This file is part of Galio.
 *
 * Galio is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * Galio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Galio. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef PAGING_H
#define PAGING_H

#include "common.h"
#include "arch/x86/cpu.h"
#include "process/process.h"

#define PAGE_SIZE             4096
#define PAGE_ENTRIES          1024
#define PAGE_TABLE_SLOTS      2048
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

#define PAGE_ALIGN_DOWN(x)    ((uintptr_t)(x) & ~(uintptr_t)(PAGE_SIZE - 1))
#define PAGE_ALIGN_UP(x)      (((uintptr_t)(x) + PAGE_SIZE - 1) & ~(uintptr_t)(PAGE_SIZE - 1))

typedef struct {
    uintptr_t *directory;
    uintptr_t *pdpt;
    uintptr_t *page_directory;
    uintptr_t *tables[PAGE_TABLE_SLOTS];
} page_directory_t;

typedef enum {
    PAGE_FAULT_UNHANDLED = 0,
    PAGE_FAULT_HANDLED = 1
} paging_fault_result_t;

void paging_init(void);
page_directory_t *paging_create_directory(void);
page_directory_t *paging_create_user_directory(void);
void paging_map(page_directory_t *pd, uintptr_t vaddr, uintptr_t paddr, u32 flags);
void paging_unmap(page_directory_t *pd, uintptr_t vaddr);
uintptr_t paging_get_physical(page_directory_t *pd, uintptr_t vaddr);
u8 paging_validate_user_range(page_directory_t *pd, uintptr_t vaddr, uintptr_t length, u8 write);
void paging_enable(page_directory_t *pd);
page_directory_t *paging_get_current(void);
page_directory_t *paging_clone_directory(page_directory_t *src);
void paging_load_directory(page_directory_t *pd);
paging_fault_result_t paging_handle_page_fault(registers_t *regs);

/* Kernel mapping helpers - allow mapping/unmapping into the global kernel page directory
 * These are used by process management to create per-process kernel stacks with
 * an unmapped guard page. They operate on the internal kernel page directory.
 */
void paging_map_kernel(uintptr_t vaddr, uintptr_t paddr, u32 flags);
void paging_unmap_kernel(uintptr_t vaddr);
#endif /* PAGING_H */
