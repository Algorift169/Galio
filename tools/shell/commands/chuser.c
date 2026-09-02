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

/* chuser.c - Change username (requires root privilege) */
#include "kprintf.h"
#include "auth.h"
#include "common.h"
#include "process.h"
#include "vga.h"
#include "string.h"

extern void shell_refresh_prompt(void);

u8 shell_chuser_command(const char *args, const char *current_dir) {
    (void)current_dir;
    
    if (!session_current()->authenticated) {
        vga_set_color(0x0C);
        kprintf("[CHUSER] Error: not authenticated\n");
        vga_set_color(0x0A);
        return 0;
    }
    
    if (session_current()->uid != UID_ROOT) {
        vga_set_color(0x0C);
        kprintf("[CHUSER] Error: root privilege required (use 'rex chuser <new_username>')\n");
        vga_set_color(0x0A);
        return 0;
    }
    
    if (!args || args[0] == 0) {
        vga_set_color(0x0C);
        kprintf("[CHUSER] Error: username required\n");
        kprintf("[CHUSER] Usage: rex chuser <new_username>\n");
        vga_set_color(0x0A);
        return 0;
    }
    
    /* Extract new username from args */
    char new_username[32];
    u32 i = 0;
    while (i < 31 && args[i] && args[i] != ' ' && args[i] != '\n' && args[i] != '\t') {
        new_username[i] = args[i];
        i++;
    }
    new_username[i] = 0;
    
    if (i == 0) {
        vga_set_color(0x0C);
        kprintf("[CHUSER] Error: invalid username\n");
        vga_set_color(0x0A);
        return 0;
    }
    
    /* Change username with root privilege */
    i32 result = auth_change_username(new_username, 1);
    
    if (result == 0) {
        vga_set_color(0x0A);
        kprintf("[CHUSER] Username changed to '%s'\n", new_username);
        vga_set_color(0x0A);
        shell_refresh_prompt();
        return 1;
    } else if (result == -1) {
        vga_set_color(0x0C);
        kprintf("[CHUSER] Error: not authenticated\n");
        vga_set_color(0x0A);
    } else if (result == -2) {
        vga_set_color(0x0C);
        kprintf("[CHUSER] Error: root privilege required\n");
        vga_set_color(0x0A);
    } else if (result == -3) {
        vga_set_color(0x0C);
        kprintf("[CHUSER] Error: invalid username\n");
        vga_set_color(0x0A);
    } else if (result == -4) {
        vga_set_color(0x0C);
        kprintf("[CHUSER] Error: username too long\n");
        vga_set_color(0x0A);
    } else {
        vga_set_color(0x0C);
        kprintf("[CHUSER] Error: failed to save changes (code: %d)\n", result);
        vga_set_color(0x0A);
    }
    
    return 0;
}
