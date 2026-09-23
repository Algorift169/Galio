#ifndef GALIO_BLOCK_H
#define GALIO_BLOCK_H

#include "common.h"

typedef struct block_device block_device_t;

typedef i32 (*block_read_fn)(block_device_t *device, u64 lba, u32 count, void *buffer);
typedef i32 (*block_write_fn)(block_device_t *device, u64 lba, u32 count, const void *buffer);
typedef i32 (*block_flush_fn)(block_device_t *device);

struct block_device {
    const char *name;
    u32 sector_size;
    u64 sector_count;
    block_read_fn read;
    block_write_fn write;
    block_flush_fn flush;
    void *private_data;
};

void block_device_register(block_device_t *device);
block_device_t *block_device_get(void);

static inline i32 block_read(u64 lba, u32 count, void *buffer) {
    block_device_t *device = block_device_get();
    return device && device->read ? device->read(device, lba, count, buffer) : -1;
}

static inline i32 block_write(u64 lba, u32 count, const void *buffer) {
    block_device_t *device = block_device_get();
    return device && device->write ? device->write(device, lba, count, buffer) : -1;
}

static inline i32 block_flush(void) {
    block_device_t *device = block_device_get();
    return device && device->flush ? device->flush(device) : -1;
}

#endif
