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

#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include "device.h"

#define DEVICE_MAX 64
#define DEVICE_PATH_MAX 512

int device_register(device_t *device);
int device_unregister(u32 major, u32 minor);
device_t *device_lookup(const char *name);
device_t *device_lookup_id(u32 major, u32 minor);
int device_manager_init(void);
int device_manager_populate_dev(void);

#endif /* DEVICE_MANAGER_H */
