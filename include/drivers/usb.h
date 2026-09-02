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

#ifndef USB_H
#define USB_H

#include "common.h"

#define USB_TRANSFER_CONTROL 0
#define USB_TRANSFER_BULK    2
#define USB_TRANSFER_INT     3

typedef struct {
    u8 bmRequestType;
    u8 bRequest;
    u16 wValue;
    u16 wIndex;
    u16 wLength;
} usb_ctrlrequest_t;

void usb_init(void);
int usb_initialized(void);
int usb_bulk_read(u32 bus, u32 addr, u8 endpoint, void *buffer, u32 size, u32 timeout);
int usb_bulk_write(u32 bus, u32 addr, u8 endpoint, void *buffer, u32 size, u32 timeout);
int usb_control_msg(u32 bus, u32 addr, u8 request_type, u8 request,
                    u16 value, u16 index, void *data, u16 size, u32 timeout);

#endif
