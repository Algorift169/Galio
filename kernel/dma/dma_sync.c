#include "kernel/dma/dma_internal.h"

static void dma_memory_barrier(void) {
    __asm__ volatile ("" ::: "memory");
}

dma_status_t dma_sync_for_device(dma_device_t *device, dma_buffer_t *buffer) {
    if (!device) device = &dma_default_device;
    if (!dma_is_valid(buffer) || !buffer->mapped || buffer->device != device) return DMA_ERR_NOT_MAPPED;
    dma_memory_barrier();
    buffer->state = DMA_BUFFER_MAPPED_FOR_DEVICE;
    return DMA_OK;
}

dma_status_t dma_sync_for_cpu(dma_device_t *device, dma_buffer_t *buffer) {
    if (!device) device = &dma_default_device;
    if (!dma_is_valid(buffer) || !buffer->mapped || buffer->device != device) return DMA_ERR_NOT_MAPPED;
    dma_memory_barrier();
    buffer->state = DMA_BUFFER_MAPPED_FOR_CPU;
    return DMA_OK;
}
