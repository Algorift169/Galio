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

#ifndef PCI_H
#define PCI_H

#include "common.h"

/* PCI config ports */
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

typedef struct pci_device {
    u8 bus;
    u8 device;
    u8 function;
    u16 vendor_id;
    u16 device_id;
    u8 class_id;
    u8 subclass;
    u8 prog_if;
    u8 header_type;
    u64 bars[6];
    u8 bar_is_mem[6];
    u8 irq_line;
    struct pci_device *next;
} pci_device_t;

/* PCI driver skeleton */
typedef struct pci_driver {
    u16 vendor_id; /* 0xFFFF = wildcard */
    u16 device_id; /* 0xFFFF = wildcard */
    int (*probe)(pci_device_t *dev);
    struct pci_driver *next;
} pci_driver_t;

/* Initialize PCI subsystem (enumerate devices) */
void pci_init(void);

/* Read/write config space helpers */
u32 pci_read_config_u32(u8 bus, u8 device, u8 function, u8 offset);
void pci_write_config_u32(u8 bus, u8 device, u8 function, u8 offset, u32 value);
u16 pci_read_config_u16(u8 bus, u8 device, u8 function, u8 offset);
void pci_write_config_u16(u8 bus, u8 device, u8 function, u8 offset, u16 value);
u8  pci_read_config_u8(u8 bus, u8 device, u8 function, u8 offset);

/* Access to device list */
pci_device_t *pci_find_device(u16 vendor, u16 device);
pci_device_t *pci_device_first(void);
pci_device_t *pci_device_next(pci_device_t *cur);

/* Driver registration */
int pci_register_driver(pci_driver_t *drv);

#endif /* PCI_H */
