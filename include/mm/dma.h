#ifndef DMA_H
#define DMA_H

#include "common.h"

void *dma_alloc_coherent(u32 size, u32 *phys);
void dma_free_coherent(void *virt, u32 phys, u32 size);

#endif
