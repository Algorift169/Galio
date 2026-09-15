#include "kernel/dma/dma.h"
#include "kprintf.h"

void dma_test(void) {
    dma_device_t device;
    dma_buffer_t buffer;
    dma_addr_t address = 0u;

    dma_device_init(&device, 0xFFFFFFFFull, 4096u, 64u,
                    DMA_DEVICE_SUPPORTS_TO_DEVICE |
                    DMA_DEVICE_SUPPORTS_FROM_DEVICE |
                    DMA_DEVICE_SUPPORTS_BIDIRECTIONAL);

    if (dma_alloc_buffer(&device, 4096u, 64u, DMA_FROM_DEVICE, &buffer) != DMA_OK) {
        kprintf("[KTEST FAIL] dma_test: allocation failed\n");
        return;
    }
    if (dma_map(&device, &buffer, DMA_FROM_DEVICE, &address) != DMA_OK ||
        address != buffer.dma_addr) {
        kprintf("[KTEST FAIL] dma_test: mapping failed\n");
        dma_free_buffer(&buffer);
        return;
    }
    dma_unmap(&device, &buffer, DMA_FROM_DEVICE);
    if (dma_free_buffer(&buffer) != DMA_OK) {
        kprintf("[KTEST FAIL] dma_test: free failed\n");
        return;
    }
    kprintf("[KTEST] dma_test passed\n");
}