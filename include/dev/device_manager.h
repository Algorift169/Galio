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
