#include "kernel/dma/dma_internal.h"

bool dma_validate_buffer_for_map(const dma_device_t *device,
                                 const dma_buffer_t *buffer,
                                 dma_direction_t direction) {
    return dma_validate_buffer_internal(device, buffer, direction) == DMA_OK;
}

dma_status_t dma_validate_buffer_internal(const dma_device_t *device,
                                          const dma_buffer_t *buffer,
                                          dma_direction_t direction) {
    if (!device || !dma_is_valid(buffer) || !dma_valid_direction(direction)) return DMA_ERR_INVALID;
    if (buffer->mapped || buffer->state != DMA_BUFFER_UNMAPPED) return DMA_ERR_MAPPED;
    if (device->max_transfer && buffer->size > device->max_transfer) return DMA_ERR_INVALID;
    if (!dma_validate_address(buffer->dma_addr, buffer->size, device->dma_mask)) return DMA_ERR_MASK;
    if (device->alignment && (buffer->dma_addr % device->alignment) != 0u) return DMA_ERR_ALIGNMENT;
    if (device->boundary &&
        ((buffer->dma_addr / device->boundary) !=
         ((buffer->dma_addr + buffer->size - 1u) / device->boundary))) {
        return DMA_ERR_UNSUPPORTED;
    }
    if (direction == DMA_TO_DEVICE && !(device->flags & DMA_DEVICE_SUPPORTS_TO_DEVICE)) return DMA_ERR_UNSUPPORTED;
    if (direction == DMA_FROM_DEVICE && !(device->flags & DMA_DEVICE_SUPPORTS_FROM_DEVICE)) return DMA_ERR_UNSUPPORTED;
    if (direction == DMA_BIDIRECTIONAL && !(device->flags & DMA_DEVICE_SUPPORTS_BIDIRECTIONAL)) return DMA_ERR_UNSUPPORTED;
    return DMA_OK;
}

dma_status_t dma_map(dma_device_t *device, dma_buffer_t *buffer,
                     dma_direction_t direction, dma_addr_t *dma_address) {
    dma_status_t status;
    if (!device) device = &dma_default_device;
    status = dma_validate_buffer_internal(device, buffer, direction);
    if (status != DMA_OK) return status;
    buffer->device = device;
    buffer->direction = direction;
    buffer->mapped = true;
    buffer->state = DMA_BUFFER_MAPPED_FOR_DEVICE;
    if (dma_address) *dma_address = buffer->dma_addr;
    return DMA_OK;
}

dma_status_t dma_unmap(dma_device_t *device, dma_buffer_t *buffer,
                       dma_direction_t direction) {
    if (!device) device = &dma_default_device;
    if (!dma_is_valid(buffer) || !buffer->mapped) return DMA_ERR_NOT_MAPPED;
    if (buffer->device != device || buffer->direction != direction) return DMA_ERR_INVALID;
    buffer->mapped = false;
    buffer->state = DMA_BUFFER_UNMAPPED;
    return DMA_OK;
}
