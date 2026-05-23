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
    
    /* Map to kernel virtual address space */
    page_directory_t *pd = paging_get_current();
    u32 vaddr = 0xF0000000; /* High memory area for DMA */
    
    for (u32 i = 0; i < pages; i++) {
        paging_map(pd, vaddr + i * PAGE_SIZE, paddr + i * PAGE_SIZE, 
                  PAGE_PRESENT | PAGE_RW);
    }
    
    return (void *)vaddr;
}

/* Free DMA-coherent memory */
void dma_free_coherent(void *virt, u32 phys, u32 size) {
    u32 pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    pmem_free(phys, pages);
    
    /* Unmap from kernel space */
    page_directory_t *pd = paging_get_current();
    for (u32 i = 0; i < pages; i++) {
        paging_unmap(pd, (u32)virt + i * PAGE_SIZE);
    }
}
