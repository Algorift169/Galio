#include "kernel/dma/dma_internal.h"

#include "kprintf.h"

void dma_debug_error(const char *message) {
    kprintf("[DMA] ERROR: %s\n", message ? message : "invalid DMA operation");
}

void dma_self_test(void) {
    dma_device_t device;
    dma_device_t limited_device;
    dma_buffer_t buffer;
    dma_buffer_t rejected_buffer;
    dma_addr_t address = 0u;
    dma_status_t status;
    u32 passed = 0u;
    u32 failed = 0u;

#define DMA_TEST(condition, message) do { \
    if (condition) { passed++; } else { failed++; kprintf("[DMA] self-test FAIL: %s\n", message); } \
} while (0)

    dma_device_init(&device, 0xFFFFFFFFull, 0u, 64u,
                    DMA_DEVICE_SUPPORTS_TO_DEVICE |
                    DMA_DEVICE_SUPPORTS_FROM_DEVICE |
                    DMA_DEVICE_SUPPORTS_BIDIRECTIONAL);
    memset(&buffer, 0, sizeof(buffer));
    status = dma_alloc_buffer(&device, DMA_PAGE_SIZE, 64u, DMA_FROM_DEVICE, &buffer);
    DMA_TEST(status == DMA_OK, "allocation");
    DMA_TEST(status == DMA_OK && buffer.phys_addr != 0u, "physical address");
    DMA_TEST(status == DMA_OK && (buffer.phys_addr & 63u) == 0u, "alignment");
    DMA_TEST(!dma_validate_range(0u, 1u, device.dma_mask), "zero address rejection");
    DMA_TEST(!dma_validate_range(UINTPTR_MAX - 1u, 4u, device.dma_mask), "overflow rejection");
    DMA_TEST(dma_alloc_buffer(&device, 0u, 64u, DMA_FROM_DEVICE, &rejected_buffer) == DMA_ERR_INVALID,
             "zero-size rejection");
    DMA_TEST(dma_alloc_buffer(&device, DMA_PAGE_SIZE, 3u, DMA_FROM_DEVICE, &rejected_buffer) == DMA_ERR_ALIGNMENT,
             "alignment rejection");
    DMA_TEST(dma_alloc_buffer(&device, SIZE_MAX, 64u, DMA_FROM_DEVICE, &rejected_buffer) == DMA_ERR_OVERFLOW,
             "size overflow rejection");
    dma_device_init(&limited_device, 0xFFFFu, 0u, 64u,
                    DMA_DEVICE_SUPPORTS_FROM_DEVICE);
    memset(&rejected_buffer, 0, sizeof(rejected_buffer));
    DMA_TEST(dma_alloc_buffer(&limited_device, DMA_PAGE_SIZE, 64u,
                              DMA_FROM_DEVICE, &rejected_buffer) == DMA_ERR_MASK,
             "device mask rejection");

    if (status == DMA_OK) {
        status = dma_map(&device, &buffer, DMA_FROM_DEVICE, &address);
        DMA_TEST(status == DMA_OK && address == buffer.dma_addr, "mapping");
        DMA_TEST(dma_map(&device, &buffer, DMA_FROM_DEVICE, NULL) == DMA_ERR_MAPPED, "double mapping rejection");
        DMA_TEST(dma_sync_for_cpu(&device, &buffer) == DMA_OK, "CPU synchronization");
        DMA_TEST(dma_sync_for_device(&device, &buffer) == DMA_OK, "device synchronization");
        DMA_TEST(dma_unmap(&device, &buffer, DMA_FROM_DEVICE) == DMA_OK, "unmapping");
        DMA_TEST(dma_unmap(&device, &buffer, DMA_FROM_DEVICE) == DMA_ERR_NOT_MAPPED, "double unmapping rejection");
        DMA_TEST(dma_free_buffer(&buffer) == DMA_OK, "free");
        DMA_TEST(dma_free_buffer(&buffer) == DMA_ERR_INVALID, "double free rejection");
    }

    kprintf("[DMA] self-test: %u passed, %u failed\n", passed, failed);
#undef DMA_TEST
}
