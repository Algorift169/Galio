#include "gpu.h"
#include "framebuffer.h"
#include "pci.h"
#include "paging.h"
#include "cpu.h"
#include "kprintf.h"

#define VBE_INDEX_PORT 0x01CE
#define VBE_DATA_PORT  0x01CF

#define VBE_INDEX_ID       0
#define VBE_INDEX_XRES     1
#define VBE_INDEX_YRES     2
#define VBE_INDEX_BPP      3
#define VBE_INDEX_ENABLE   4
#define VBE_ENABLE         0x01
#define VBE_LFB_ENABLED    0x40
#define VBE_NOCLEARMEM     0x80
#define VBE_ID_MIN         0xB0C0
#define VBE_ID_MAX         0xB0C5
#define VBE_LFB_BASE       0xE0000000u
#define GPU_MMIO_BASE      0xD0000000u

static void vbe_write(u16 index, u16 value) {
    outw(VBE_INDEX_PORT, index);
    outw(VBE_DATA_PORT, value);
}

static u16 vbe_read(u16 index) {
    outw(VBE_INDEX_PORT, index);
    return inw(VBE_DATA_PORT);
}

static u8 gpu_map_mmio(const pci_device_t *device, u8 bar, u32 *mapped_size) {
    u64 size;
    u32 pages;

    if (!device || bar >= 6 || !device->bar_is_mem[bar] ||
        device->bars[bar] > 0xFFFFFFFFu) return 0;
    size = pci_get_bar_size(device, bar);
    if (!size || size > 0x01000000u) return 0;
    pages = (u32)((size + PAGE_SIZE - 1) / PAGE_SIZE);
    for (u32 i = 0; i < pages; i++) {
        paging_map_kernel(GPU_MMIO_BASE + i * PAGE_SIZE,
                          (u32)device->bars[bar] + i * PAGE_SIZE,
                          PAGE_PRESENT | PAGE_RW | PAGE_NOCACHE);
    }
    if (mapped_size) *mapped_size = pages * PAGE_SIZE;
    return 1;
}

static u8 gpu_try_qemu_vbe(void) {
    u16 id = vbe_read(VBE_INDEX_ID);
    if (id < VBE_ID_MIN || id > VBE_ID_MAX) return 0;

    vbe_write(VBE_INDEX_ENABLE, 0);
    vbe_write(VBE_INDEX_XRES, FB_DEFAULT_WIDTH);
    vbe_write(VBE_INDEX_YRES, FB_DEFAULT_HEIGHT);
    vbe_write(VBE_INDEX_BPP, FB_DEFAULT_BPP);
    vbe_write(VBE_INDEX_ENABLE, VBE_ENABLE | VBE_LFB_ENABLED | VBE_NOCLEARMEM);

    if (!fb_attach(VBE_LFB_BASE, FB_DEFAULT_WIDTH, FB_DEFAULT_HEIGHT,
                   FB_DEFAULT_PITCH, FB_DEFAULT_BPP)) {
        vbe_write(VBE_INDEX_ENABLE, 0);
        return 0;
    }
    kprintf("GPU: QEMU Bochs VBE %04X, %ux%u framebuffer at 0x%08X\n",
             id, FB_DEFAULT_WIDTH, FB_DEFAULT_HEIGHT, VBE_LFB_BASE);
    return 1;
}

void gpu_init(void) {
    u8 vbe_enabled = 0;
    u8 pci_gpu_found = 0;
    for (pci_device_t *device = pci_device_first(); device;
         device = pci_device_next(device)) {
        if (device->class_id != 0x03) continue;
        pci_gpu_found = 1;
        kprintf("GPU: PCI VGA %04X:%04X class %02X:%02X\n",
                 device->vendor_id, device->device_id,
                 device->class_id, device->subclass);
        for (u8 bar = 0; bar < 6; bar++) {
            if (!device->bars[bar]) continue;
            kprintf("GPU: BAR%u %s phys=0x%llX size=0x%llX\n", bar,
                    device->bar_is_mem[bar] ? "MEM" : "IO",
                    (unsigned long long)device->bars[bar],
                    (unsigned long long)pci_get_bar_size(device, bar));
        }
                u32 mmio_size = 0;
                if (gpu_map_mmio(device, 0, &mmio_size)) {
                    kprintf("GPU: BAR0 mapped uncached at 0x%08X (%u bytes)\n",
                        GPU_MMIO_BASE, mmio_size);
                }
        /* Keep VBE modesetting disabled until framebuffer console output is
         * available; the kernel console currently renders through VGA text. */
        if (device->vendor_id == 0x8086) {
            kprintf("GPU: Intel display controller detected; generation-specific driver required\n");
        }
    }
    if (!vbe_enabled && !pci_gpu_found) {
        kprintf("GPU: no framebuffer-capable device found; using VGA text\n");
    } else if (!vbe_enabled) {
        kprintf("GPU: PCI device detected; vendor framebuffer driver unavailable\n");
    }
}