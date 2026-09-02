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

#include "panel/fs_browser.h"
#include "vga.h"
#include "lib/string.h"
#include "kprintf.h"
#include <string.h>
#include "fs/ext2.h"
#include "heap.h"

#define FS_BROWSER_MAX_ENTRIES 512

static fs_browser_t fs_browser = {
    .entries = NULL,
    .count = 0,
    .scroll_offset = 0,
    .max_entries = FS_BROWSER_MAX_ENTRIES
};

void fs_browser_init(void) {
    if (fs_browser.entries != NULL) {
        return;  /* Already initialized */
    }
    
    fs_browser.entries = (fs_entry_t *)kmalloc(sizeof(fs_entry_t) * FS_BROWSER_MAX_ENTRIES);
    if (!fs_browser.entries) {
        return;  /* kmalloc failed - gracefully degrade */
    }
    
    fs_browser.count = 0;
    fs_browser.scroll_offset = 0;
    fs_browser_load_dir("/");
}

u32 fs_browser_load_dir(const char *path) {
    if (!fs_browser.entries) return 0;
    
    /* Load filesystem entries - simplified version that lists root filesystem */
    fs_browser.count = 0;
    fs_browser.scroll_offset = 0;
    
    /* Common filesystem directories */
    const char *common_dirs[] = {
        "bin", "boot", "dev", "etc", "home", "lib", "media", "mnt",
        "opt", "proc", "root", "run", "sbin", "srv", "sys", "tmp",
        "usr", "var", NULL
    };
    
    for (int i = 0; common_dirs[i] != NULL && fs_browser.count < FS_BROWSER_MAX_ENTRIES; i++) {
        strncpy(fs_browser.entries[fs_browser.count].path, common_dirs[i], 
                sizeof(fs_browser.entries[fs_browser.count].path) - 1);
        fs_browser.entries[fs_browser.count].path[sizeof(fs_browser.entries[fs_browser.count].path) - 1] = '\0';
        fs_browser.entries[fs_browser.count].is_dir = 1;
        fs_browser.count++;
    }
    
    return fs_browser.count;
}

fs_browser_t *fs_browser_get(void) {
    return &fs_browser;
}

void fs_browser_scroll_up(void) {
    if (fs_browser.scroll_offset > 0) {
        fs_browser.scroll_offset--;
    }
}

void fs_browser_scroll_down(void) {
    /* Allow scrolling only if there are more entries to display */
    if (fs_browser.scroll_offset < (fs_browser.count > 12 ? fs_browser.count - 12 : 0)) {
        fs_browser.scroll_offset++;
    }
}

void fs_browser_draw(int x, int y, int width, int height) {
    if (!fs_browser.entries) {
        return;  /* Not initialized, skip drawing */
    }
    
    vga_set_color(0x0F);  /* White text */
    
    /* Draw visible entries */
    u32 display_count = (height < (fs_browser.count - fs_browser.scroll_offset)) ? 
                        height : (fs_browser.count - fs_browser.scroll_offset);
    
    for (u32 i = 0; i < display_count && i < height; i++) {
        u32 entry_idx = fs_browser.scroll_offset + i;
        if (entry_idx >= fs_browser.count) break;
        
        int pos_x = x;
        int pos_y = y + i;
        
        /* Draw directory indicator if applicable */
        if (fs_browser.entries[entry_idx].is_dir) {
            vga_write_cell(pos_x++, pos_y, '[', 0x0F);
        }
        
        /* Draw the name, truncated to width */
        const char *name = fs_browser.entries[entry_idx].path;
        int max_len = width - (fs_browser.entries[entry_idx].is_dir ? 3 : 1);
        
        for (int j = 0; name[j] && j < max_len && pos_x < x + width; j++) {
            vga_write_cell(pos_x++, pos_y, name[j], 0x0F);
        }
        
        if (fs_browser.entries[entry_idx].is_dir && pos_x < x + width) {
            vga_write_cell(pos_x++, pos_y, ']', 0x0F);
        }
        
        /* Clear remaining space in line */
        while (pos_x < x + width) {
            vga_write_cell(pos_x++, pos_y, ' ', 0x00);
        }
    }
    
    /* Clear remaining lines if needed */
    for (u32 i = display_count; i < height; i++) {
        for (int j = 0; j < width; j++) {
            vga_write_cell(x + j, y + i, ' ', 0x00);
        }
    }
}

u32 fs_browser_get_count(void) {
    return fs_browser.count;
}

u32 fs_browser_get_scroll_offset(void) {
    return fs_browser.scroll_offset;
}
