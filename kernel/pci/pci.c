/*
 * Galio Kernel
 *
 * Copyright (C) 2026 Israfil [Your Legal Name]
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

/* internal lists */
static pci_device_t *pci_dev_list = NULL;
static pci_driver_t *pci_drv_list = NULL;

static u32 pci_config_address(u8 bus, u8 device, u8 function, u8 offset) {
    u32 address = (u32)((u32)1 << 31) | ((u32)bus << 16) | ((u32)device << 11) |
                    ((u32)function << 8) | (offset & 0xFC);
    return address;
}

u32 pci_read_config_u32(u8 bus, u8 device, u8 function, u8 offset) {
    u32 addr = pci_config_address(bus, device, function, offset);
    outl(PCI_CONFIG_ADDRESS, addr);
    return inl(PCI_CONFIG_DATA);
}

void pci_write_config_u32(u8 bus, u8 device, u8 function, u8 offset, u32 value) {
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
                /* read BARs */
                for (int i = 0; i < 6; i++) {
                    u32 bar = pci_read_config_u32(bus, dev, fn, 0x10 + i * 4);
                    pd->bars[i] = bar;
                    if ((bar & 0x1) == 0) {
                        pd->bar_is_mem[i] = 1;
                        /* 64-bit addressing simple handling deferred */
                    } else {
                        pd->bar_is_mem[i] = 0; /* I/O space */
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
