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

/* dma.c - DMA-coherent memory allocation */

#include "mm/dma.h"
#include "pmem.h"
#include "paging.h"
#include "lib/kprintf.h"
#include "lib/string.h"

#define PAGE_SIZE 4096

/* Allocate DMA-coherent memory (physically contiguous) */
void *dma_alloc_coherent(u32 size, u32 *phys) {
    u32 pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    u32 paddr = pmem_alloc(pages);
    if (!paddr) return NULL;
    
    if (phys) *phys = paddr;
    return (void *)(uintptr_t)paddr;
}

/* Free DMA-coherent memory */
void dma_free_coherent(void *virt, u32 phys, u32 size) {
    (void)virt;
    u32 pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    pmem_free(phys, pages);
}
