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

#include "dev/device_manager.h"
#include "vfs_core.h"
#include "vfs.h"
#include "keyboard.h"
#include "mouse/mouse.h"
#include "vga.h"
#include "kprintf.h"
#include "string.h"

#define DEV_MAJOR_CORE 1
#define DEV_MAJOR_INPUT 2
#define DEV_NULL_MINOR 0
#define DEV_CONSOLE_MINOR 1
#define DEV_KEYBOARD_MINOR 0
#define DEV_MOUSE_MINOR 1

static device_t *devices[DEVICE_MAX];
static u8 device_manager_ready;

static int null_open(device_t *device) { (void)device; return 0; }
static int null_close(device_t *device) { (void)device; return 0; }
static device_ssize_t null_read(device_t *device, void *buffer, size_t count) {
    (void)device; (void)buffer; (void)count; return 0;
}
static device_ssize_t null_write(device_t *device, const void *buffer, size_t count) {
    (void)device; (void)buffer; return (device_ssize_t)count;
}

static device_ssize_t console_write(device_t *device, const void *buffer, size_t count) {
    (void)device;
    if (!buffer) return -1;
    const char *text = (const char *)buffer;
    for (size_t i = 0; i < count; i++) vga_putch(text[i]);
    return (device_ssize_t)count;
}

static device_ssize_t keyboard_read(device_t *device, void *buffer, size_t count) {
    (void)device;
    if (!buffer || count < sizeof(u32) * 3) return -1;
    u8 scancode, pressed, extended;
    if (!keyboard_read_event(&scancode, &pressed, &extended)) return 0;
    u8 *out = (u8 *)buffer;
    out[0] = scancode;
    out[1] = pressed;
    out[2] = extended;
    return sizeof(u32) * 3;
}

static device_ssize_t mouse_read(device_t *device, void *buffer, size_t count) {
    (void)device;
    if (!buffer || count < sizeof(mouse_event_t)) return -1;
    mouse_event_t *event = (mouse_event_t *)buffer;
    if (!mouse_read_event(event)) return 0;
    return (device_ssize_t)sizeof(*event);
}

static const device_ops_t null_ops = { null_open, null_close, null_read, null_write, NULL, NULL };
static const device_ops_t console_ops = { null_open, null_close, NULL, console_write, NULL, NULL };
static const device_ops_t keyboard_ops = { null_open, null_close, keyboard_read, NULL, NULL, NULL };
static const device_ops_t mouse_ops = { null_open, null_close, mouse_read, NULL, NULL, NULL };

static device_t null_device = { "null", DEVICE_CHAR, DEV_MAJOR_CORE, DEV_NULL_MINOR, &null_ops, NULL, 0 };
static device_t console_device = { "console", DEVICE_TTY, DEV_MAJOR_CORE, DEV_CONSOLE_MINOR, &console_ops, NULL, 0 };
static device_t keyboard_device = { "keyboard0", DEVICE_INPUT, DEV_MAJOR_INPUT, DEV_KEYBOARD_MINOR, &keyboard_ops, NULL, 0 };
static device_t mouse_device = { "mouse0", DEVICE_INPUT, DEV_MAJOR_INPUT, DEV_MOUSE_MINOR, &mouse_ops, NULL, 0 };

int device_register(device_t *device) {
    if (!device || device->name[0] == 0 || !device->ops) return -1;
    if (device_lookup_id(device->major, device->minor)) return -1;
    for (u32 i = 0; i < DEVICE_MAX; i++) {
        if (!devices[i]) {
            devices[i] = device;
            kprintf("[DEV] Registered %s device: %s (%u:%u)\n",
                    device->type == DEVICE_INPUT ? "input" : "char",
                    device->name, device->major, device->minor);
            return 0;
        }
    }
    return -1;
}

int device_unregister(u32 major, u32 minor) {
    for (u32 i = 0; i < DEVICE_MAX; i++) {
        if (devices[i] && devices[i]->major == major && devices[i]->minor == minor) {
            devices[i] = NULL;
            return 0;
        }
    }
    return -1;
}

device_t *device_lookup(const char *name) {
    if (!name) return NULL;
    for (u32 i = 0; i < DEVICE_MAX; i++) {
        if (devices[i] && strcmp(devices[i]->name, name) == 0) return devices[i];
    }
    return NULL;
}

device_t *device_lookup_id(u32 major, u32 minor) {
    for (u32 i = 0; i < DEVICE_MAX; i++) {
        if (devices[i] && devices[i]->major == major && devices[i]->minor == minor) return devices[i];
    }
    return NULL;
}

static void create_dev_node(const char *path, u32 major, u32 minor, u32 mode) {
    if (vfs_core_lookup(path, 0)) return;
    if (vfs_core_create_device_ex(path, mode, major, minor)) {
        kprintf("[DEV] Created %s\n", path);
    }
}

int device_manager_populate_dev(void) {
    if (!vfs_core_lookup("./dev", 0)) vfs_core_create_dir("./dev", 1);
    create_dev_node("./dev/null", DEV_MAJOR_CORE, DEV_NULL_MINOR, 0666);
    create_dev_node("./dev/console", DEV_MAJOR_CORE, DEV_CONSOLE_MINOR, 0622);
    create_dev_node("./dev/keyboard", DEV_MAJOR_INPUT, DEV_KEYBOARD_MINOR, 0444);
    create_dev_node("./dev/mouse", DEV_MAJOR_INPUT, DEV_MOUSE_MINOR, 0444);
    return 0;
}

int device_manager_init(void) {
    if (device_manager_ready) return 0;
    if (device_register(&null_device) || device_register(&console_device) ||
        device_register(&keyboard_device) || device_register(&mouse_device)) return -1;
    device_manager_ready = 1;
    kprintf("[DEV] Device subsystem initialized\n");
    return device_manager_populate_dev();
}
