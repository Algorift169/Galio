#ifndef KERNEL_DMA_H
#define KERNEL_DMA_H

#include "kernel/dma/dma_types.h"

void dma_init(void);
void dma_device_init(dma_device_t *device, uint64_t dma_mask,
                     size_t max_transfer, size_t alignment,
                     uint32_t flags);
dma_status_t dma_set_mask(dma_device_t *device, uint64_t dma_mask);

dma_status_t dma_alloc_buffer(dma_device_t *device, size_t size,
                               size_t alignment, dma_direction_t direction,
                               dma_buffer_t *buffer);
dma_status_t dma_free_buffer(dma_buffer_t *buffer);

dma_status_t dma_map(dma_device_t *device, dma_buffer_t *buffer,
                     dma_direction_t direction, dma_addr_t *dma_address);
dma_status_t dma_unmap(dma_device_t *device, dma_buffer_t *buffer,
                       dma_direction_t direction);
dma_status_t dma_sync_for_device(dma_device_t *device, dma_buffer_t *buffer);
dma_status_t dma_sync_for_cpu(dma_device_t *device, dma_buffer_t *buffer);

bool dma_is_valid(const dma_buffer_t *buffer);
bool dma_validate_range(uintptr_t address, size_t size, uint64_t dma_mask);
bool dma_validate_address(dma_addr_t address, size_t size, uint64_t dma_mask);

void dma_self_test(void);

#endif /* KERNEL_DMA_H */
