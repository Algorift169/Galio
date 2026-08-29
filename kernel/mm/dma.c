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
