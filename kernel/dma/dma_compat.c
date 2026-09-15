#include "mm/dma.h"
#include "kernel/dma/dma.h"
#include "kernel/dma/dma_internal.h"
#include "string.h"

#define DMA_COMPAT_MAX DMA_MAX_BUFFERS

typedef struct {
    dma_buffer_t buffer;
    bool used;
} dma_compat_entry_t;

static dma_compat_entry_t entries[DMA_COMPAT_MAX];

void *dma_alloc_coherent(u32 size, u32 *phys) {
    for (u32 i = 0u; i < DMA_COMPAT_MAX; i++) {
        if (!entries[i].used) {
            dma_status_t status = dma_alloc_buffer(NULL, size, DMA_PAGE_SIZE,
                                                   DMA_BIDIRECTIONAL,
                                                   &entries[i].buffer);
            if (status != DMA_OK) return NULL;
            status = dma_map(NULL, &entries[i].buffer, DMA_BIDIRECTIONAL, NULL);
            if (status != DMA_OK) {
                dma_free_buffer(&entries[i].buffer);
                return NULL;
            }
            entries[i].used = true;
            if (phys) *phys = (u32)entries[i].buffer.dma_addr;
            return entries[i].buffer.vaddr;
        }
    }
    return NULL;
}

void dma_free_coherent(void *virt, u32 phys, u32 size) {
    (void)size;
    for (u32 i = 0u; i < DMA_COMPAT_MAX; i++) {
        if (entries[i].used && entries[i].buffer.vaddr == virt) {
            if (phys != 0u && entries[i].buffer.dma_addr != (dma_addr_t)phys) return;
            dma_unmap(NULL, &entries[i].buffer, DMA_BIDIRECTIONAL);
            dma_free_buffer(&entries[i].buffer);
            memset(&entries[i], 0, sizeof(entries[i]));
            return;
        }
    }
}
