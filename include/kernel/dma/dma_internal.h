#ifndef KERNEL_DMA_INTERNAL_H
#define KERNEL_DMA_INTERNAL_H

#include "kernel/dma/dma.h"

#define DMA_MAX_BUFFERS 128u
#define DMA_PHYSICAL_LIMIT 0xFFFFFFFFull

extern dma_buffer_t *dma_buffer_registry[DMA_MAX_BUFFERS];
extern dma_device_t dma_default_device;
extern bool dma_subsystem_ready;

bool dma_valid_direction(dma_direction_t direction);
dma_status_t dma_validate_buffer_internal(const dma_device_t *device,
                                          const dma_buffer_t *buffer,
                                          dma_direction_t direction);
dma_buffer_t *dma_find_buffer(void *vaddr);
void dma_debug_error(const char *message);
dma_status_t dma_prepare_allocation(const dma_device_t *device, size_t size,
                                    size_t alignment, dma_direction_t direction,
                                    size_t *allocated_size, size_t *pages,
                                    u32 *max_address);

#endif /* KERNEL_DMA_INTERNAL_H */
