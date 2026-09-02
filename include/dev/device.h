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
