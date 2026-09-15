#include "kernel/dma/dma_internal.h"

#define DMA_MIN_PHYSICAL_ADDRESS 0x00100000u

static bool dma_is_power_of_two(size_t value) {
    return value != 0u && (value & (value - 1u)) == 0u;
}

dma_status_t dma_prepare_allocation(const dma_device_t *device, size_t size,
                                    size_t alignment, dma_direction_t direction,
                                    size_t *allocated_size, size_t *pages,
                                    u32 *max_address) {
    uint64_t mask;

    if (!device || !allocated_size || !pages || !max_address ||
        size == 0u || !dma_valid_direction(direction)) return DMA_ERR_INVALID;
    if (device->max_transfer && size > device->max_transfer) return DMA_ERR_INVALID;
    if (alignment == 0u) alignment = device->alignment;
    if (!dma_is_power_of_two(alignment) || alignment > DMA_PAGE_SIZE) return DMA_ERR_ALIGNMENT;
    if (size - 1u > (size_t)-1 - DMA_PAGE_SIZE) return DMA_ERR_OVERFLOW;

    *allocated_size = (size + DMA_PAGE_SIZE - 1u) & ~(size_t)(DMA_PAGE_SIZE - 1u);
    *pages = *allocated_size / DMA_PAGE_SIZE;
    if (*pages == 0u) return DMA_ERR_OVERFLOW;

    mask = device->dma_mask;
    if (mask > DMA_PHYSICAL_LIMIT) mask = DMA_PHYSICAL_LIMIT;
    if (mask < DMA_MIN_PHYSICAL_ADDRESS || *allocated_size - 1u > (size_t)mask) {
        return DMA_ERR_MASK;
    }
    if (device->boundary && *allocated_size > device->boundary) return DMA_ERR_UNSUPPORTED;

    /* The physical allocator currently exposes 32-bit physical addresses. */
    *max_address = (u32)mask;
    return DMA_OK;
}