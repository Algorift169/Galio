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
    .bytes_per_pixel = 4u,
    .bytes = 0,
    .physical_base = 0,
    .base = (volatile u8 *)FB_LINEAR_BASE,
    .initialized = 0
};

static u8 fb_mask_valid(u8 position, u8 size, u32 bpp) {
    return size > 0u && size <= 8u && position < bpp && position + size <= bpp;
}

static u32 fb_pack_channel(u32 value, u8 size) {
    if (size >= 8u) {
        return value & 0xFFu;
    }
    if (size == 0u) {
        return 0u;
    }
    return (value * ((1u << size) - 1u) + 127u) / 255u;
}

static u32 fb_pixel_value(u32 color) {
    u32 pixel = 0;
    pixel |= (u32)fb_pack_channel((color >> 16) & 0xFFu, g_fb.format.red_size) << g_fb.format.red_position;
    pixel |= (u32)fb_pack_channel((color >> 8) & 0xFFu, g_fb.format.green_size) << g_fb.format.green_position;
    pixel |= (u32)fb_pack_channel(color & 0xFFu, g_fb.format.blue_size) << g_fb.format.blue_position;
    if (g_fb.format.reserved_size != 0u) {
        pixel |= (u32)fb_pack_channel((color >> 24) & 0xFFu, g_fb.format.reserved_size) << g_fb.format.reserved_position;
    }
    return pixel;
}

static void fb_write_pixel_raw(u32 x, u32 y, u32 value) {
    u64 offset;
    if (!g_fb.bytes || !g_fb.bytes_per_pixel ||
        x >= g_fb.width || y >= g_fb.height) return;
    offset = (u64)y * g_fb.pitch + (u64)x * g_fb.bytes_per_pixel;
    if (offset >= g_fb.bytes || (u64)g_fb.bytes_per_pixel > g_fb.bytes - offset) return;
    volatile u8 *pixel = g_fb.base + offset;
    if (g_fb.bytes_per_pixel == 4u) {
        *(volatile u32 *)pixel = value;
        return;
    }
    if (g_fb.bytes_per_pixel == 3u) {
        pixel[0] = (u8)(value & 0xFFu);
        pixel[1] = (u8)((value >> 8) & 0xFFu);
        pixel[2] = (u8)((value >> 16) & 0xFFu);
        return;
    }
    if (g_fb.bytes_per_pixel == 2u) {
        *(volatile u16 *)pixel = (u16)value;
        return;
    }
    pixel[0] = (u8)(value & 0xFFu);
}

static u32 fb_read_pixel_raw(u32 x, u32 y) {
    u64 offset;
    if (!g_fb.bytes || !g_fb.bytes_per_pixel ||
        x >= g_fb.width || y >= g_fb.height) return 0u;
    offset = (u64)y * g_fb.pitch + (u64)x * g_fb.bytes_per_pixel;
    if (offset >= g_fb.bytes || (u64)g_fb.bytes_per_pixel > g_fb.bytes - offset) return 0u;
    volatile u8 *pixel = g_fb.base + offset;
    if (g_fb.bytes_per_pixel == 4u) {
        return *(volatile u32 *)pixel;
    }
    if (g_fb.bytes_per_pixel == 3u) {
        return (u32)pixel[0] | ((u32)pixel[1] << 8) | ((u32)pixel[2] << 16);
    }
    if (g_fb.bytes_per_pixel == 2u) {
        return *(volatile u16 *)pixel;
    }
    return (u32)pixel[0];
}

static u64 fb_pixel_address(u32 x, u32 y) {
    return (u64)y * g_fb.pitch + (u64)x * g_fb.bytes_per_pixel;
}

u32 fb_make_color(u8 r, u8 g, u8 b, u8 a) {
    u32 pixel = 0u;
    if (!g_fb.initialized) {
        return (u32)((((u32)a & 0xFFu) << 24) | ((u32)r << 16) | ((u32)g << 8) | (u32)b);
    }
    pixel |= (u32)fb_pack_channel(r, g_fb.format.red_size) << g_fb.format.red_position;
    pixel |= (u32)fb_pack_channel(g, g_fb.format.green_size) << g_fb.format.green_position;
    pixel |= (u32)fb_pack_channel(b, g_fb.format.blue_size) << g_fb.format.blue_position;
    if (g_fb.format.reserved_size != 0u) {
        pixel |= (u32)fb_pack_channel(a, g_fb.format.reserved_size) << g_fb.format.reserved_position;
    }
    return pixel;
}

void fb_init(void) {
    g_fb.initialized = 0;
    g_fb.base = NULL;
    g_fb.width = FB_DEFAULT_WIDTH;
    g_fb.height = FB_DEFAULT_HEIGHT;
    g_fb.pitch = FB_DEFAULT_PITCH;
    g_fb.bpp = FB_DEFAULT_BPP;
    g_fb.bytes_per_pixel = 4u;
    g_fb.physical_base = 0;
    g_fb.bytes = 0;
    g_fb.format.red_position = 16u;
    g_fb.format.red_size = 8u;
    g_fb.format.green_position = 8u;
    g_fb.format.green_size = 8u;
    g_fb.format.blue_position = 0u;
    g_fb.format.blue_size = 8u;
    g_fb.format.reserved_position = 24u;
    g_fb.format.reserved_size = 8u;
    fb_console_init();
}

u8 fb_validate_geometry(u32 width, u32 height, u32 pitch, u32 bpp, u64 *bytes_out) {
    u64 row_bytes;
    u64 bytes;

    if (!width || !height || bpp == 0u || (bpp % 8u) != 0u) {
        return 0;
    }
    if (bpp != 16u && bpp != 24u && bpp != 32u) {
        return 0;
    }

    row_bytes = (u64)width * ((u64)bpp / 8u);
    bytes = (u64)pitch * (u64)height;
    if (row_bytes == 0u || bytes == 0u || pitch < (u32)row_bytes) {
        return 0;
    }
    if (bytes > UINT64_MAX - 4096ull) {
        return 0;
    }
    if (bytes_out) {
        *bytes_out = bytes;
    }
    return 1;
}

u8 fb_init_from_multiboot(const void *multiboot_info) {
    const u32 *words = (const u32 *)multiboot_info;
    u64 physical_base;
    u32 pitch;
    u32 width;
    u32 height;
    u32 bpp;

    if (!multiboot_info || !words) {
        kprintf("[FB] No usable framebuffer available\n");
        return 0;
    }
    if (!(words[0] & (1u << 12))) {
        kprintf("[FB] No usable framebuffer available (multiboot flag missing)\n");
        return 0;
    }

    physical_base = ((u64)words[23] << 32) | words[22];
    pitch = words[24];
    width = words[25];
    height = words[26];
    bpp = ((const u8 *)multiboot_info)[108];

    /* GRUB reports the VGA text buffer as 80x25x16 in text mode. It is not a
     * pixel framebuffer and writing pixels there corrupts the visible console. */
    if (physical_base == 0xB8000ull && width == 80u && height == 25u) {
        kprintf("[FB] Text-mode VGA buffer detected; keeping VGA console\n");
        return 0;
    }

    if (!fb_attach(physical_base, width, height, pitch, bpp)) {
        kprintf("[FB] Invalid framebuffer descriptor: phys=0x%016llX %ux%u pitch=%u bpp=%u\n",
                (unsigned long long)physical_base, width, height, pitch, bpp);
        return 0;
    }

    g_fb.format.red_position = ((const u8 *)multiboot_info)[112];
    g_fb.format.red_size = ((const u8 *)multiboot_info)[113];
    g_fb.format.green_position = ((const u8 *)multiboot_info)[116];
    g_fb.format.green_size = ((const u8 *)multiboot_info)[117];
    g_fb.format.blue_position = ((const u8 *)multiboot_info)[120];
    g_fb.format.blue_size = ((const u8 *)multiboot_info)[121];
    g_fb.format.reserved_position = ((const u8 *)multiboot_info)[124];
    g_fb.format.reserved_size = ((const u8 *)multiboot_info)[125];

    if (!fb_mask_valid(g_fb.format.red_position, g_fb.format.red_size, bpp) ||
        !fb_mask_valid(g_fb.format.green_position, g_fb.format.green_size, bpp) ||
        !fb_mask_valid(g_fb.format.blue_position, g_fb.format.blue_size, bpp)) {
        kprintf("[FB] Unsupported channel format for %u bpp; defaulting to RGB888\n", bpp);
        g_fb.format.red_position = 16u;
        g_fb.format.red_size = 8u;
        g_fb.format.green_position = 8u;
        g_fb.format.green_size = 8u;
        g_fb.format.blue_position = 0u;
        g_fb.format.blue_size = 8u;
        g_fb.format.reserved_position = 24u;
        g_fb.format.reserved_size = 8u;
    }

    if (bpp != 32u && bpp != 24u && bpp != 16u) {
        kprintf("[FB] Unsupported pixel depth %u; falling back to the existing serial console\n", bpp);
        return 0;
    }

    kprintf("[FB] Framebuffer initialized\n");
    kprintf("[FB] Address: 0x%016llX\n", (unsigned long long)physical_base);
    kprintf("[FB] Resolution: %ux%u\n", width, height);
    kprintf("[FB] Pitch: %u\n", pitch);
    kprintf("[FB] BPP: %u\n", bpp);
    kprintf("[FB] Bytes per pixel: %u\n", g_fb.bytes_per_pixel);
    kprintf("[FB] Size: %llu bytes\n", (unsigned long long)g_fb.bytes);
    fb_console_init();
    return 1;
}

u8 fb_is_initialized(void) {
    return g_fb.initialized;
}

u8 fb_attach(u64 physical_base, u32 width, u32 height, u32 pitch, u32 bpp) {
    u64 bytes = 0;
    u64 mapped_bytes;
    u64 physical_page;
    u64 page_offset;
    u32 pages;

    if (!physical_base || !fb_validate_geometry(width, height, pitch, bpp, &bytes)) {
        return 0;
    }

    g_fb.bytes_per_pixel = (u32)(bpp / 8u);
    physical_page = physical_base & ~(u64)(PAGE_SIZE - 1u);
    page_offset = physical_base & (u64)(PAGE_SIZE - 1u);
    if (bytes > UINT64_MAX - page_offset) {
        return 0;
    }
    mapped_bytes = bytes + page_offset;
    pages = (u32)((mapped_bytes + PAGE_SIZE - 1ull) / PAGE_SIZE);
    if (pages > 0x100000u) {
        return 0;
    }
    for (u32 i = 0; i < pages; i++) {
        paging_map_kernel(FB_LINEAR_BASE + i * PAGE_SIZE, physical_page + i * PAGE_SIZE,
                          PAGE_PRESENT | PAGE_RW | PAGE_NOCACHE);
    }

    g_fb.width = width;
    g_fb.height = height;
    g_fb.pitch = pitch;
    g_fb.bpp = bpp;
    g_fb.bytes = bytes;
    g_fb.physical_base = physical_base;
    g_fb.base = (volatile u8 *)(uintptr_t)(FB_LINEAR_BASE + page_offset);
    g_fb.format.red_position = 16u;
    g_fb.format.red_size = 8u;
    g_fb.format.green_position = 8u;
    g_fb.format.green_size = 8u;
    g_fb.format.blue_position = 0u;
    g_fb.format.blue_size = 8u;
    g_fb.format.reserved_position = 24u;
    g_fb.format.reserved_size = 8u;
    g_fb.initialized = 1;
    fb_clear(FB_COLOR(0, 0, 0));
    return 1;
}

void fb_set_mode(u32 width, u32 height, u32 bpp) {
    if (!width || !height || !bpp) {
        return;
    }
    if (!fb_validate_geometry(width, height, width * (bpp / 8u), bpp, &g_fb.bytes)) {
        return;
    }

    g_fb.width = width;
    g_fb.height = height;
    g_fb.bpp = bpp;
    g_fb.bytes_per_pixel = (u32)(bpp / 8u);
    g_fb.pitch = width * g_fb.bytes_per_pixel;
    g_fb.base = (volatile u8 *)FB_LINEAR_BASE;
    g_fb.initialized = 1;
    fb_clear(FB_COLOR(0, 0, 0));
}

void fb_clear(u32 color) {
    if (!g_fb.initialized || !g_fb.base) {
        return;
    }
    fb_fill_rect(0u, 0u, g_fb.width, g_fb.height, color);
}

void fb_put_pixel(u32 x, u32 y, u32 color) {
    if (!g_fb.initialized || !g_fb.base) {
        return;
    }
    if (x >= g_fb.width || y >= g_fb.height) {
        return;
    }

    fb_write_pixel_raw(x, y, fb_make_color((u8)((color >> 16) & 0xFFu),
                                           (u8)((color >> 8) & 0xFFu),
                                           (u8)(color & 0xFFu),
                                           (u8)((color >> 24) & 0xFFu)));
}

u32 fb_get_pixel(u32 x, u32 y) {
    u32 pixel;
    if (!g_fb.initialized || !g_fb.base) {
        return 0u;
    }
    if (x >= g_fb.width || y >= g_fb.height) {
        return 0u;
    }

    pixel = fb_read_pixel_raw(x, y);
    return pixel;
}

void fb_fill_rect(u32 x, u32 y, u32 width, u32 height, u32 color) {
    u32 x_end;
    u32 y_end;
    if (!g_fb.initialized || !g_fb.base) {
        return;
    }
    if (width == 0u || height == 0u) {
        return;
    }
    if (x >= g_fb.width || y >= g_fb.height) {
        return;
    }

    x_end = (x + width > g_fb.width) ? g_fb.width : x + width;
    y_end = (y + height > g_fb.height) ? g_fb.height : y + height;
    for (u32 py = y; py < y_end; py++) {
        for (u32 px = x; px < x_end; px++) {
            fb_write_pixel_raw(px, py, fb_make_color((u8)((color >> 16) & 0xFFu),
                                                     (u8)((color >> 8) & 0xFFu),
                                                     (u8)(color & 0xFFu),
                                                     (u8)((color >> 24) & 0xFFu)));
        }
    }
}

void fb_draw_hline(u32 x, u32 y, u32 width, u32 color) {
    fb_fill_rect(x, y, width, 1u, color);
}

void fb_draw_vline(u32 x, u32 y, u32 height, u32 color) {
    fb_fill_rect(x, y, 1u, height, color);
}

void fb_draw_line(u32 x0, u32 y0, u32 x1, u32 y1, u32 color) {
    s32 dx;
    s32 dy;
    s32 sx;
    s32 sy;
    s32 err;
    s32 e2;

    if (!g_fb.initialized || !g_fb.base) {
        return;
    }
    dx = (s32)x1 - (s32)x0;
    dy = (s32)y1 - (s32)y0;
    sx = dx < 0 ? -1 : 1;
    sy = dy < 0 ? -1 : 1;
    dx = dx < 0 ? -dx : dx;
    dy = dy < 0 ? -dy : dy;
    err = dx - dy;

    while (1) {
        if (x0 < g_fb.width && y0 < g_fb.height) {
            fb_put_pixel(x0, y0, color);
        }
        if (x0 == x1 && y0 == y1) {
            break;
        }
        e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 = (u32)((s32)x0 + sx);
        }
        if (e2 < dx) {
            err += dx;
            y0 = (u32)((s32)y0 + sy);
        }
    }
}

void fb_draw_rect(u32 x, u32 y, u32 width, u32 height, u32 color) {
    fb_draw_hline(x, y, width, color);
    fb_draw_hline(x, y + height - 1u, width, color);
    fb_draw_vline(x, y, height, color);
    fb_draw_vline(x + width - 1u, y, height, color);
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
