#include "drivers/block.h"

static block_device_t *active_block_device;

void block_device_register(block_device_t *device) {
    if (!active_block_device && device && device->read && device->write) {
        active_block_device = device;
    }
}

block_device_t *block_device_get(void) {
    return active_block_device;
}
