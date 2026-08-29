#ifndef DEVICE_H
#define DEVICE_H

#include "common.h"
#include <stddef.h>

typedef struct device device_t;

typedef enum {
    DEVICE_CHAR = 1,
    DEVICE_BLOCK,
    DEVICE_INPUT,
    DEVICE_TTY,
    DEVICE_PSEUDO
} device_type_t;

typedef s64 device_ssize_t;

typedef struct {
    int (*open)(device_t *device);
    int (*close)(device_t *device);
    device_ssize_t (*read)(device_t *device, void *buffer, size_t count);
    device_ssize_t (*write)(device_t *device, const void *buffer, size_t count);
    int (*ioctl)(device_t *device, u64 request, void *arg);
    int (*poll)(device_t *device);
} device_ops_t;

struct device {
    char name[32];
    device_type_t type;
    u32 major;
    u32 minor;
    const device_ops_t *ops;
    void *private_data;
    u32 open_count;
};

#endif /* DEVICE_H */
