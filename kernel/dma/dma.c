#include "kernel/dma/dma_internal.h"

#include "kprintf.h"
#include "pmem.h"
#include "paging.h"
#include "string.h"

#define DMA_MIN_PHYSICAL_ADDRESS 0x00100000u

dma_buffer_t *dma_buffer_registry[DMA_MAX_BUFFERS];
dma_device_t dma_default_device;
bool dma_subsystem_ready;

bool dma_valid_direction(dma_direction_t direction) {
    return direction == DMA_TO_DEVICE ||
           direction == DMA_FROM_DEVICE ||
           direction == DMA_BIDIRECTIONAL;
}

void dma_device_init(dma_device_t *device, uint64_t dma_mask,
                     size_t max_transfer, size_t alignment,
                     uint32_t flags) {
    if (!device) return;
    device->dma_mask = dma_mask;
    device->max_transfer = max_transfer;
    device->alignment = alignment ? alignment : DMA_PAGE_SIZE;
    device->boundary = 0u;
    device->flags = flags;
}

void dma_init(void) {
    memset(dma_buffer_registry, 0, sizeof(dma_buffer_registry));
    dma_device_init(&dma_default_device, UINT64_MAX, 0u, DMA_PAGE_SIZE,
                    DMA_DEVICE_SUPPORTS_TO_DEVICE |
                    DMA_DEVICE_SUPPORTS_FROM_DEVICE |
                    DMA_DEVICE_SUPPORTS_BIDIRECTIONAL);
    dma_subsystem_ready = true;
    kprintf("[DMA] init: contiguous identity-mapped DMA buffers, x86 cache coherent\n");
}

dma_status_t dma_set_mask(dma_device_t *device, uint64_t dma_mask) {
    if (!device || dma_mask == 0u) return DMA_ERR_INVALID;
    device->dma_mask = dma_mask;
    return DMA_OK;
}

bool dma_validate_range(uintptr_t address, size_t size, uint64_t dma_mask) {
    uintptr_t end;
    if (address == 0u || size == 0u) return false;
    if (size - 1u > (uintptr_t)-1 - address) return false;
    end = address + size - 1u;
    if ((uint64_t)end > DMA_PHYSICAL_LIMIT || (uint64_t)end > dma_mask) return false;
    return true;
}

bool dma_validate_address(dma_addr_t address, size_t size, uint64_t dma_mask) {
    return dma_validate_range((uintptr_t)address, size, dma_mask);
}

static u32 find_free_slot(void) {
    for (u32 i = 0u; i < DMA_MAX_BUFFERS; i++) {
        if (!dma_buffer_registry[i]) return i;
    }
    return DMA_MAX_BUFFERS;
}

static bool direction_supported(const dma_device_t *device,
                                dma_direction_t direction) {
    if (!device) return false;
    if (direction == DMA_TO_DEVICE) return (device->flags & DMA_DEVICE_SUPPORTS_TO_DEVICE) != 0u;
    if (direction == DMA_FROM_DEVICE) return (device->flags & DMA_DEVICE_SUPPORTS_FROM_DEVICE) != 0u;
    return (device->flags & DMA_DEVICE_SUPPORTS_BIDIRECTIONAL) != 0u;
}

dma_status_t dma_alloc_buffer(dma_device_t *device, size_t size,
                               size_t alignment, dma_direction_t direction,
                               dma_buffer_t *buffer) {
    u32 slot;
    size_t pages;
    size_t allocated_size;
    u32 max_address;
    u32 physical;
    dma_status_t allocation_status;

    if (!dma_subsystem_ready || !buffer || size == 0u || !dma_valid_direction(direction)) {
        return DMA_ERR_INVALID;
    }
    if (!device) device = &dma_default_device;
    if (!direction_supported(device, direction)) return DMA_ERR_UNSUPPORTED;
    allocation_status = dma_prepare_allocation(device, size, alignment, direction,
                                                &allocated_size, &pages, &max_address);
    if (allocation_status != DMA_OK) return allocation_status;

    if (buffer->allocated) return DMA_ERR_BUSY;
    slot = find_free_slot();
    if (slot == DMA_MAX_BUFFERS) return DMA_ERR_NOMEM;
    physical = pmem_alloc_region(pages, DMA_MIN_PHYSICAL_ADDRESS, max_address);
    if (!physical || !dma_validate_range((uintptr_t)physical, size, device->dma_mask)) {
        if (physical) pmem_free(physical, pages);
        return physical ? DMA_ERR_MASK : DMA_ERR_NOMEM;
    }

    memset(buffer, 0, sizeof(*buffer));
    buffer->vaddr = (void *)(uintptr_t)physical;
    buffer->phys_addr = (uintptr_t)physical;
    buffer->dma_addr = (dma_addr_t)physical;
    buffer->size = size;
    buffer->alignment = alignment;
    buffer->allocated_size = allocated_size;
    buffer->direction = direction;
    buffer->flags = DMA_BUFFER_ALLOCATED | DMA_BUFFER_COHERENT;
    buffer->state = DMA_BUFFER_UNMAPPED;
    buffer->allocated = true;
    buffer->device = device;
    dma_buffer_registry[slot] = buffer;
    return DMA_OK;
}

dma_buffer_t *dma_find_buffer(void *vaddr) {
    if (!vaddr) return NULL;
    for (u32 i = 0u; i < DMA_MAX_BUFFERS; i++) {
        if (dma_buffer_registry[i] && dma_buffer_registry[i]->vaddr == vaddr) {
            return dma_buffer_registry[i];
        }
    }
    return NULL;
}

bool dma_is_valid(const dma_buffer_t *buffer) {
    return dma_subsystem_ready && buffer && dma_find_buffer(buffer->vaddr) == buffer &&
           buffer->allocated &&
           (buffer->flags & DMA_BUFFER_ALLOCATED) != 0u &&
           buffer->vaddr != NULL && buffer->phys_addr != 0u && buffer->size != 0u &&
           dma_validate_range(buffer->phys_addr, buffer->size,
                              buffer->device ? buffer->device->dma_mask : UINT64_MAX);
}

dma_status_t dma_free_buffer(dma_buffer_t *buffer) {
    if (!dma_is_valid(buffer)) return DMA_ERR_INVALID;
    if (buffer->mapped || buffer->state != DMA_BUFFER_UNMAPPED) {
        dma_debug_error("buffer freed while mapped");
        return DMA_ERR_BUSY;
    }
    if (dma_find_buffer(buffer->vaddr) != buffer) return DMA_ERR_INVALID;
    for (u32 i = 0u; i < DMA_MAX_BUFFERS; i++) {
        if (dma_buffer_registry[i] == buffer) dma_buffer_registry[i] = NULL;
    }
    pmem_free((u32)buffer->phys_addr, buffer->allocated_size / DMA_PAGE_SIZE);
    memset(buffer, 0, sizeof(*buffer));
    return DMA_OK;
}
