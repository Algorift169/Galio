/*
 * Galio Kernel
 *
 * Copyright (C) 2026 S.M Israfil
 *
 * This file is part of Galio.
 *
 * Galio is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * Galio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Galio. If not, see <https://www.gnu.org/licenses/>.
 */

#include "pci.h"
#include "lib/kprintf.h"
#include "lib/string.h"
#include "arch/x86/cpu.h"
#include "mm/heap.h"
#include "mm/paging.h"
#include "kernel/dma/dma.h"
#include "arch/x86/apic.h"
#include "acpi/acpi.h"

/* internal lists */
static pci_device_t *pci_dev_list = NULL;
static pci_driver_t *pci_drv_list = NULL;

static u32 pci_ecam_read32(u8 bus, u8 device, u8 function, u8 offset) {
    u64 base = acpi_mcfg_base();
    u64 address;
    volatile u32 *register_address;
    if (!base || bus == 0u) return 0xFFFFFFFFu;
    address = base + ((u64)bus << 20) + ((u64)device << 15) +
              ((u64)function << 12) + (offset & 0xFFCu);
    register_address = (volatile u32 *)mmio_map_physical(address, PAGE_SIZE);
    return register_address ? *register_address : 0xFFFFFFFFu;
}

static u32 pci_config_address(u8 bus, u8 device, u8 function, u8 offset) {
    u32 address = (u32)((u32)1 << 31) | ((u32)bus << 16) | ((u32)device << 11) |
                    ((u32)function << 8) | (offset & 0xFC);
    return address;
}

u32 pci_read_config_u32(u8 bus, u8 device, u8 function, u8 offset) {
    if (acpi_mcfg_base() && bus != 0u) return pci_ecam_read32(bus, device, function, offset);
    u32 addr = pci_config_address(bus, device, function, offset);
    outl(PCI_CONFIG_ADDRESS, addr);
    return inl(PCI_CONFIG_DATA);
}

void pci_write_config_u32(u8 bus, u8 device, u8 function, u8 offset, u32 value) {
    if (acpi_mcfg_base() && bus != 0u) {
        u64 address = acpi_mcfg_base() + ((u64)bus << 20) + ((u64)device << 15) +
                      ((u64)function << 12) + (offset & 0xFFCu);
        volatile u32 *register_address = (volatile u32 *)mmio_map_physical(address, PAGE_SIZE);
        if (register_address) *register_address = value;
        return;
    }
    u32 addr = pci_config_address(bus, device, function, offset);
    outl(PCI_CONFIG_ADDRESS, addr);
    outl(PCI_CONFIG_DATA, value);
}

u16 pci_read_config_u16(u8 bus, u8 device, u8 function, u8 offset) {
    u32 v = pci_read_config_u32(bus, device, function, offset & 0xFC);
    u16 res = (u16)((v >> ((offset & 2) * 8)) & 0xFFFF);
    return res;
}

void pci_write_config_u16(u8 bus, u8 device, u8 function, u8 offset, u16 value) {
    u32 aligned = offset & 0xFC;
    u32 shift = (offset & 2) * 8;
    u32 mask = 0xFFFFu << shift;
    u32 orig = pci_read_config_u32(bus, device, function, aligned);
    u32 updated = (orig & ~mask) | ((u32)value << shift);
    pci_write_config_u32(bus, device, function, aligned, updated);
}

u8 pci_read_config_u8(u8 bus, u8 device, u8 function, u8 offset) {
    u32 v = pci_read_config_u32(bus, device, function, offset & 0xFC);
    u8 res = (u8)((v >> ((offset & 3) * 8)) & 0xFF);
    return res;
}

u64 pci_get_bar_size(const pci_device_t *device, u8 bar_index) {
    u32 original;
    u32 mask;
    u32 original_high = 0;
    u32 mask_high = 0;
    u64 size;
    u8 offset;
    u8 is_64_bit = 0;

    if (!device || bar_index >= 6 || !device->bars[bar_index]) return 0;
    offset = (u8)(0x10 + bar_index * 4);
    original = pci_read_config_u32(device->bus, device->device,
                                   device->function, offset);
    if (device->bar_is_mem[bar_index] &&
        ((original >> 1) & 0x3u) == 0x2u && bar_index < 5) {
        is_64_bit = 1;
        original_high = pci_read_config_u32(device->bus, device->device,
                                            device->function, offset + 4);
    }
    pci_write_config_u32(device->bus, device->device, device->function,
                         offset, 0xFFFFFFFFu);
    if (is_64_bit) {
        pci_write_config_u32(device->bus, device->device, device->function,
                             offset + 4, 0xFFFFFFFFu);
    }
    mask = pci_read_config_u32(device->bus, device->device,
                               device->function, offset);
    if (is_64_bit) {
        mask_high = pci_read_config_u32(device->bus, device->device,
                                        device->function, offset + 4);
    }
    pci_write_config_u32(device->bus, device->device, device->function,
                         offset, original);
    if (is_64_bit) {
        pci_write_config_u32(device->bus, device->device, device->function,
                             offset + 4, original_high);
    }
    if (mask == 0 || mask == 0xFFFFFFFFu) return 0;
    if (is_64_bit) {
        size = ((u64)mask_high << 32) | (mask & ~0xFu);
        return (~size) + 1;
    }
    if (device->bar_is_mem[bar_index]) {
        return (u64)(~(mask & ~0xFu) + 1u);
    }
    return (u64)(~(mask & ~0x3u) + 1u);
}

static void pci_add_device(pci_device_t *d) {
    d->next = pci_dev_list;
    pci_dev_list = d;
}

static void pci_enumerate_bus(void) {
    for (u32 bus = 0; bus < 256; bus++) {
        for (u8 dev = 0; dev < 32; dev++) {
            for (u8 fn = 0; fn < 8; fn++) {
                u16 vendor = pci_read_config_u16(bus, dev, fn, 0x00);
                if (vendor == 0xFFFF) {
                    if (fn == 0) break; /* no device at this slot */
                    else continue;
                }
                pci_device_t *pd = kmalloc(sizeof(pci_device_t));
                if (!pd) return;
                memset(pd, 0, sizeof(*pd));
                pd->bus = bus; pd->device = dev; pd->function = fn;
                pd->vendor_id = vendor;
                pd->device_id = pci_read_config_u16(bus, dev, fn, 0x02);
                pd->prog_if = pci_read_config_u8(bus, dev, fn, 0x09);
                pd->subclass = pci_read_config_u8(bus, dev, fn, 0x0A);
                pd->class_id = pci_read_config_u8(bus, dev, fn, 0x0B);
                pd->header_type = pci_read_config_u8(bus, dev, fn, 0x0E);
                pd->irq_line = pci_read_config_u8(bus, dev, fn, 0x3C);
                dma_device_init(&pd->dma, 0xFFFFFFFFull, 0u, 1u,
                                DMA_DEVICE_SUPPORTS_TO_DEVICE |
                                DMA_DEVICE_SUPPORTS_FROM_DEVICE |
                                DMA_DEVICE_SUPPORTS_BIDIRECTIONAL);
                /* read BARs */
                for (int i = 0; i < 6; i++) {
                    u32 bar = pci_read_config_u32(bus, dev, fn, 0x10 + i * 4);
                    if ((bar & 0x1) == 0) {
                        pd->bar_is_mem[i] = 1;
                        if (((bar >> 1) & 0x3) == 0x2 && i < 5) {
                            u32 high = pci_read_config_u32(bus, dev, fn, 0x14 + i * 4);
                            pd->bars[i] = ((u64)high << 32) | (bar & ~0xFu);
                            i++;
                        } else {
                            pd->bars[i] = bar & ~0xFu;
                        }
                    } else {
                        pd->bar_is_mem[i] = 0;
                        pd->bars[i] = bar & ~0x3u;
                    }
                }
                pci_add_device(pd);
                /* if function 0 and not multi-function, don't probe other functions */
                if (fn == 0) {
                    u8 hdr = pd->header_type;
                    if ((hdr & 0x80) == 0) break;
                }
            }
        }
    }
}

/* Probe existing devices with a driver */
static void pci_probe_driver_on_devices(pci_driver_t *drv) {
    pci_device_t *it = pci_dev_list;
    while (it) {
        if ((drv->vendor_id == 0xFFFF || drv->vendor_id == it->vendor_id) &&
            (drv->device_id == 0xFFFF || drv->device_id == it->device_id)) {
            if (drv->probe) drv->probe(it);
        }
        it = it->next;
    }
}

int pci_register_driver(pci_driver_t *drv) {
    if (!drv) return -1;
    drv->next = pci_drv_list;
    pci_drv_list = drv;
    /* probe existing devices now */
    pci_probe_driver_on_devices(drv);
    return 0;
}

int pci_enable_msi(pci_device_t *device, u32 vector) {
    u8 capability;
    u16 control;
    u16 command;

    if (!device || !apic_is_available() || vector < 32u || vector >= 256u) return -1;
    capability = pci_read_config_u8(device->bus, device->device,
                                    device->function, 0x34);
    while (capability >= 0x40u && capability < 0xFCu &&
           pci_read_config_u8(device->bus, device->device,
                              device->function, capability) != 0x05u) {
        capability = pci_read_config_u8(device->bus, device->device,
                                        device->function, capability + 1u);
    }
    if (capability < 0x40u || capability >= 0xFCu ||
        pci_read_config_u8(device->bus, device->device,
                           device->function, capability) != 0x05u) return -1;
    control = pci_read_config_u16(device->bus, device->device,
                                  device->function, capability + 2u);
    pci_write_config_u32(device->bus, device->device, device->function,
                         capability + 4u, 0xFEE00000u | (apic_cpu_id() << 12));
    if (control & (1u << 7)) {
        pci_write_config_u32(device->bus, device->device, device->function,
                             capability + 8u, 0u);
        pci_write_config_u16(device->bus, device->device, device->function,
                             capability + 12u, (u16)vector);
    } else {
        pci_write_config_u16(device->bus, device->device, device->function,
                             capability + 8u, (u16)vector);
    }
    control |= 1u;
    pci_write_config_u16(device->bus, device->device, device->function,
                         capability + 2u, control);
    command = pci_read_config_u16(device->bus, device->device,
                                  device->function, 0x04);
    pci_write_config_u16(device->bus, device->device, device->function,
                         0x04, command | 0x06u);
    return 0;
}

pci_device_t *pci_find_device(u16 vendor, u16 device) {
    pci_device_t *it = pci_dev_list;
    while (it) {
        if (it->vendor_id == vendor && it->device_id == device) return it;
        it = it->next;
    }
    return NULL;
}

pci_device_t *pci_device_first(void) { return pci_dev_list; }
pci_device_t *pci_device_next(pci_device_t *cur) { return cur ? cur->next : NULL; }

void pci_init(void) {
    kprintf("PCI: Enumerating devices...\n");
    pci_enumerate_bus();
    /* Probe drivers already registered */
    pci_driver_t *drv = pci_drv_list;
    while (drv) {
        pci_probe_driver_on_devices(drv);
        drv = drv->next;
    }
}
