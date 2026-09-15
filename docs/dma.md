# Galio DMA subsystem

Galio DMA buffers keep three address domains separate:

- `vaddr` is the CPU virtual pointer.
- `phys_addr` is the physical address returned by the physical memory manager.
- `dma_addr` is the address programmed into a device. It is currently equal to
  the physical address because Galio has no IOMMU mapping layer yet.

Drivers should create a `dma_device_t` with the device's DMA mask, transfer
limit, alignment, boundary, and supported directions. They then allocate a
`dma_buffer_t`, map it with an explicit direction, and pass only `dma_addr` to
hardware. A mapped buffer must be unmapped before it is freed.

The current implementation allocates physically contiguous pages from Galio's
physical memory manager. The existing x86_64 kernel uses identity mapping for
the low physical range used by this allocator, so the CPU pointer is derived
from that established mapping, not treated as a general virtual-to-physical
cast. DMA addresses are checked for overflow, ownership, alignment, boundary,
physical-range, and device-mask violations before mapping.

x86 cache coherence means no cache flush instruction is required by the
current architecture. `dma_sync_for_device()` and `dma_sync_for_cpu()` still
issue compiler memory barriers and track ownership state, leaving room for an
architecture-specific cache implementation later.

This provides DMA memory management and validation only. The e1000 driver still
uses the compatibility coherent-allocation wrapper and has not been converted
to the explicit buffer API, so this subsystem does not by itself claim a
validated hardware DMA data path.