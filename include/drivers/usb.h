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
int usb_bulk_read(u32 bus, u32 addr, u8 endpoint, void *buffer, u32 size, u32 timeout);
int usb_bulk_write(u32 bus, u32 addr, u8 endpoint, void *buffer, u32 size, u32 timeout);
int usb_control_msg(u32 bus, u32 addr, u8 request_type, u8 request,
                    u16 value, u16 index, void *data, u16 size, u32 timeout);

#endif
