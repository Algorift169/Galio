#ifndef KERNEL_DMA_TYPES_H
#define KERNEL_DMA_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common.h"

#ifndef DMA_PAGE_SIZE
#define DMA_PAGE_SIZE 4096u
#endif

typedef uint64_t dma_addr_t;

typedef enum {
    DMA_TO_DEVICE = 1,
    DMA_FROM_DEVICE = 2,
    DMA_BIDIRECTIONAL = 3
} dma_direction_t;

typedef enum {
    DMA_OK = 0,
    DMA_ERR_INVALID = -1,
    DMA_ERR_NOMEM = -2,
    DMA_ERR_OVERFLOW = -3,
    DMA_ERR_ALIGNMENT = -4,
    DMA_ERR_MASK = -5,
    DMA_ERR_MAPPED = -6,
    DMA_ERR_NOT_MAPPED = -7,
    DMA_ERR_BUSY = -8,
    DMA_ERR_UNSUPPORTED = -9
} dma_status_t;

typedef enum {
    DMA_BUFFER_UNMAPPED = 0,
    DMA_BUFFER_MAPPED_FOR_DEVICE,
    DMA_BUFFER_MAPPED_FOR_CPU,
    DMA_BUFFER_ERROR
} dma_buffer_state_t;

#define DMA_DEVICE_SUPPORTS_TO_DEVICE   0x00000001u
#define DMA_DEVICE_SUPPORTS_FROM_DEVICE 0x00000002u
#define DMA_DEVICE_SUPPORTS_BIDIRECTIONAL 0x00000004u

#define DMA_BUFFER_ALLOCATED 0x00000001u
#define DMA_BUFFER_COHERENT  0x00000002u
#define DMA_BUFFER_LEGACY    0x00000004u

typedef struct dma_device {
    uint64_t dma_mask;
    size_t max_transfer;
    size_t alignment;
    size_t boundary;
    uint32_t flags;
} dma_device_t;

typedef struct dma_buffer {
    void *vaddr;
    uintptr_t phys_addr;
    dma_addr_t dma_addr;
    size_t size;
    size_t alignment;
    size_t allocated_size;
    dma_direction_t direction;
    uint32_t flags;
    dma_buffer_state_t state;
    bool mapped;
    bool allocated;
    dma_device_t *device;
} dma_buffer_t;

typedef struct dma_segment {
    dma_addr_t addr;
    size_t length;
} dma_segment_t;

#endif /* KERNEL_DMA_TYPES_H */
