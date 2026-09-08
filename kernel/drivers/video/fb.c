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

#include "framebuffer.h"
#include "paging.h"
#include "kprintf.h"
#include "fb_console.h"

#define FB_LINEAR_BASE 0xE0000000u
#define FB_PAGE_MASK   (PAGE_SIZE - 1u)

static framebuffer_t g_fb = {
    .width = FB_DEFAULT_WIDTH,
    .height = FB_DEFAULT_HEIGHT,
    .pitch = FB_DEFAULT_PITCH,
    .bpp = FB_DEFAULT_BPP,
    .base = (volatile u8 *)FB_LINEAR_BASE,
    .initialized = 0
};

static u8 fb_mask_valid(u8 position, u8 size, u32 bpp) {
    return size > 0 && size <= 8 && position < bpp && position + size <= bpp;
}

u8 fb_validate_geometry(u32 width, u32 height, u32 pitch, u32 bpp, u32 *bytes_out) {
    u64 row_bytes;
    u64 bytes;

    if (!width || !height || bpp != 32) return 0;
    row_bytes = (u64)width * (bpp / 8u);
    bytes = (u64)pitch * height;
    if (row_bytes > 0xFFFFFFFFu || pitch < (u32)row_bytes ||
        bytes == 0 || bytes > 0xFFFFFFFFu) return 0;
    if (bytes_out) *bytes_out = (u32)bytes;
    return 1;
}

static u32 fb_pack_channel(u32 value, u8 size) {
    if (size >= 8) return value & 0xFFu;
    return (value * ((1u << size) - 1u) + 127u) / 255u;
}

static u32 fb_pixel_value(u32 color) {
    u32 pixel = 0;
    pixel |= fb_pack_channel((color >> 16) & 0xFFu, g_fb.format.red_size) << g_fb.format.red_position;
    pixel |= fb_pack_channel((color >> 8) & 0xFFu, g_fb.format.green_size) << g_fb.format.green_position;
    pixel |= fb_pack_channel(color & 0xFFu, g_fb.format.blue_size) << g_fb.format.blue_position;
    return pixel;
}

static u32 fb_pixel_address(u32 x, u32 y) {
    return y * g_fb.pitch + x * (g_fb.bpp / 8u);
}

void fb_init(void) {
    g_fb.initialized = 0;
    g_fb.base = NULL;
    fb_console_init();
}

u8 fb_init_from_multiboot(const void *multiboot_info) {
    const u32 *words = (const u32 *)multiboot_info;
    u64 physical_base;
    u32 pitch;
    u32 width;
    u32 height;
    u32 bpp;

    if (!words) {
        kprintf("FB: no Multiboot information pointer\n");
        return 0;
    }
    if (!(words[0] & (1u << 12))) {
        kprintf("FB: Multiboot framebuffer flag not set (flags=0x%08X)\n", words[0]);
        return 0;
    }

    physical_base = ((u64)words[23] << 32) | words[22];
    pitch = words[24];
    width = words[25];
    height = words[26];
    bpp = ((const u8 *)multiboot_info)[108];
    if (physical_base > 0xFFFFFFFFu) {
        kprintf("FB: framebuffer above 4 GiB is unsupported by current paging\n");
        return 0;
    }
    if (!fb_attach((u32)physical_base, width, height, pitch, bpp)) {
        kprintf("FB: invalid framebuffer descriptor phys=0x%08X %ux%u pitch=%u bpp=%u\n",
                (u32)physical_base, width, height, pitch, bpp);
        return 0;
    }
    g_fb.format.red_position = ((const u8 *)multiboot_info)[112];
    g_fb.format.red_size = ((const u8 *)multiboot_info)[113];
    g_fb.format.green_position = ((const u8 *)multiboot_info)[116];
    g_fb.format.green_size = ((const u8 *)multiboot_info)[117];
    g_fb.format.blue_position = ((const u8 *)multiboot_info)[120];
    g_fb.format.blue_size = ((const u8 *)multiboot_info)[121];
    if (!fb_mask_valid(g_fb.format.red_position, g_fb.format.red_size, bpp) ||
        !fb_mask_valid(g_fb.format.green_position, g_fb.format.green_size, bpp) ||
        !fb_mask_valid(g_fb.format.blue_position, g_fb.format.blue_size, bpp)) {
        kprintf("FB: unsupported channel format\n");
        fb_init();
        return 0;
    }
    kprintf("FB: Multiboot framebuffer %ux%u %ubpp pitch=%u phys=0x%08X\n",
            width, height, bpp, pitch, (u32)physical_base);
    fb_console_init();
    return 1;
}

u8 fb_is_initialized(void) {
    return g_fb.initialized;
}

u8 fb_attach(u32 physical_base, u32 width, u32 height, u32 pitch, u32 bpp) {
    u32 bytes;
    u32 physical_page;
    u32 page_offset;
    u32 mapped_bytes;
    u32 pages;

    if (!physical_base || !fb_validate_geometry(width, height, pitch, bpp, &bytes)) {
        return 0;
    }

    physical_page = physical_base & ~FB_PAGE_MASK;
    page_offset = physical_base & FB_PAGE_MASK;
    if (bytes > 0xFFFFFFFFu - page_offset) return 0;
    mapped_bytes = bytes + page_offset;
    pages = (mapped_bytes + PAGE_SIZE - 1u) / PAGE_SIZE;
    if (pages > (0x10000000u / PAGE_SIZE)) return 0;
    for (u32 i = 0; i < pages; i++) {
        paging_map_kernel(FB_LINEAR_BASE + i * PAGE_SIZE, physical_page + i * PAGE_SIZE,
                          PAGE_PRESENT | PAGE_RW | PAGE_NOCACHE);
    }

    g_fb.width = width;
    g_fb.height = height;
    g_fb.bpp = bpp;
    g_fb.pitch = pitch;
    g_fb.bytes = bytes;
    g_fb.physical_base = physical_base;
    g_fb.base = (volatile u8 *)(uintptr_t)(FB_LINEAR_BASE + page_offset);
    g_fb.format.red_position = 16;
    g_fb.format.red_size = 8;
    g_fb.format.green_position = 8;
    g_fb.format.green_size = 8;
    g_fb.format.blue_position = 0;
    g_fb.format.blue_size = 8;
    g_fb.initialized = 1;
    fb_clear(FB_COLOR(0, 0, 0));
    return 1;
}

void fb_set_mode(u32 width, u32 height, u32 bpp) {
    if (width == 0 || height == 0 || bpp == 0) {
        return;
    }

    g_fb.width = width;
    g_fb.height = height;
    g_fb.bpp = bpp;
    g_fb.pitch = width * (bpp / 8u);
    g_fb.base = (volatile u8 *)FB_LINEAR_BASE;
    g_fb.initialized = 1;
    fb_clear(FB_COLOR(0, 0, 0));
}

void fb_clear(u32 color) {
    if (!g_fb.initialized || !g_fb.base) {
        return;
    }

    for (u32 y = 0; y < g_fb.height; y++) {
        for (u32 x = 0; x < g_fb.width; x++) {
            *(volatile u32 *)(g_fb.base + fb_pixel_address(x, y)) = fb_pixel_value(color);
        }
    }
}

void fb_put_pixel(u32 x, u32 y, u32 color) {
    if (!g_fb.initialized || !g_fb.base) {
        return;
    }
    if (x >= g_fb.width || y >= g_fb.height) {
        return;
    }

    *(volatile u32 *)(g_fb.base + fb_pixel_address(x, y)) = fb_pixel_value(color);
}

u32 fb_get_pixel(u32 x, u32 y) {
    if (!g_fb.initialized || !g_fb.base) {
        return 0u;
    }
    if (x >= g_fb.width || y >= g_fb.height) {
        return 0u;
    }

    return *(volatile u32 *)(g_fb.base + fb_pixel_address(x, y));
}

void fb_fill_rect(u32 x, u32 y, u32 width, u32 height, u32 color) {
    if (!g_fb.initialized || !g_fb.base) {
        return;
    }
    if (width == 0 || height == 0) {
        return;
    }
    if (x >= g_fb.width || y >= g_fb.height) {
        return;
    }

    u32 x_end = width > g_fb.width - x ? g_fb.width : x + width;
    u32 y_end = height > g_fb.height - y ? g_fb.height : y + height;

    if (x_end > g_fb.width) {
        x_end = g_fb.width;
    }
    if (y_end > g_fb.height) {
        y_end = g_fb.height;
    }

    for (u32 py = y; py < y_end; py++) {
        for (u32 px = x; px < x_end; px++) {
            *(volatile u32 *)(g_fb.base + fb_pixel_address(px, py)) = fb_pixel_value(color);
        }
    }
}

void fb_draw_hline(u32 x, u32 y, u32 width, u32 color) {
    fb_fill_rect(x, y, width, 1u, color);
}

void fb_draw_vline(u32 x, u32 y, u32 height, u32 color) {
    fb_fill_rect(x, y, 1u, height, color);
}

void fb_get_info(u32 *width, u32 *height, u32 *pitch, u32 *bpp) {
    if (width) {
        *width = g_fb.width;
    }
    if (height) {
        *height = g_fb.height;
    }
    if (pitch) {
        *pitch = g_fb.pitch;
    }
    if (bpp) {
        *bpp = g_fb.bpp;
    }
}
