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

/* passwd.c - Change user password */
#include "kprintf.h"
#include "auth.h"
#include "keyboard.h"
#include "vga.h"
#include "string.h"

#define PASSWD_INPUT_MAX 32

static void passwd_read_password(const char *prompt, char *buffer, u32 max_len) {
    if (!buffer || max_len == 0) return;
    
    kprintf("%s", prompt);
    vga_set_color(0x0A);
    
    u32 len = 0;
    for (;;) {
        u8 c = 0;
        u8 scancode;
        u8 is_pressed;
        u8 extended;
        
        while (1) {
            if (!keyboard_read_event(&scancode, &is_pressed, &extended)) {
                for (volatile int i = 0; i < 100; i++);
                continue;
            }
            
            if (!is_pressed) continue;
            if (extended) continue;
            
            c = scancode_to_ascii(scancode);
            if (c == 0 || c == '\t') continue;
            
            break;
        }
        
        if (c == '\n') {
            buffer[len] = 0;
            kprintf("\n");
            break;
        }
        
        if (c == '\b') {
            if (len > 0) {
                len--;
                kprintf("\b \b");
            }
            continue;
        }
        
        if (c >= 32 && c < 127) {
            if (len < max_len - 1) {
                buffer[len++] = c;
                vga_putch('*');
            } else {
                vga_putch('\a');
            }
        }
    }
    
    if (max_len > 0) buffer[(max_len - 1) < len ? (max_len - 1) : len] = 0;
}

u8 shell_passwd_command(const char *args, const char *current_dir) {
    (void)args;
    (void)current_dir;
    
    if (!session_current()->authenticated) {
        kprintf("[PASSWD] Error: not authenticated\n");
        return 0;
    }
    
    char old_password[PASSWD_INPUT_MAX];
    char new_password[PASSWD_INPUT_MAX];
    char confirm[PASSWD_INPUT_MAX];
    
    vga_set_color(0x0A);
    kprintf("\n[PASSWD] Change password for %s\n\n", session_current()->username);
    
    passwd_read_password("Current password: ", old_password, PASSWD_INPUT_MAX);
    
    i32 result = auth_change_password(old_password, "");
    if (result == -2) {
        vga_set_color(0x0C);
        kprintf("[PASSWD] Error: Incorrect password\n");
        vga_set_color(0x0A);
        return 0;
    }
    if (result < 0) {
        vga_set_color(0x0C);
        kprintf("[PASSWD] Error: Operation failed (code: %d)\n", result);
        vga_set_color(0x0A);
        return 0;
    }
    
    /* Prompt for new password */
    passwd_read_password("New password: ", new_password, PASSWD_INPUT_MAX);
    passwd_read_password("Confirm new password: ", confirm, PASSWD_INPUT_MAX);
    
    if (new_password[0] == 0) {
        vga_set_color(0x0C);
        kprintf("[PASSWD] Error: Password cannot be empty\n");
        vga_set_color(0x0A);
        return 0;
    }
    
    if (strcmp(new_password, confirm) != 0) {
        vga_set_color(0x0C);
        kprintf("[PASSWD] Error: Passwords do not match\n");
        vga_set_color(0x0A);
        return 0;
    }
    
    result = auth_change_password(old_password, new_password);
    if (result == 0) {
        vga_set_color(0x0A);
        kprintf("[PASSWD] Password changed successfully\n");
        vga_set_color(0x0A);
        return 1;
    } else if (result == -2) {
        vga_set_color(0x0C);
        kprintf("[PASSWD] Error: Incorrect password\n");
        vga_set_color(0x0A);
    } else {
        vga_set_color(0x0C);
        kprintf("[PASSWD] Error: Failed to save password (code: %d)\n", result);
        vga_set_color(0x0A);
    }
    
    return 0;
}
