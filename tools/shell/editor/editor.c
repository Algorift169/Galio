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

#include "editor.h"
#include "vga.h"
#include "kprintf.h"
#include "string.h"
#include "vfs.h"
#include "vfs_core.h"
#include "keyboard.h"

#define EDITOR_BUFFER_SIZE 4096

typedef struct {
    char content[EDITOR_BUFFER_SIZE];
    u32 size;
    u32 cursor;
} editor_buffer_t;

/* ASCII tables */
static const u8 ascii_table[] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b','\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n',0, 'a','s',
    'd','f','g','h','j','k','l',';','\'','`',0,'\\','z','x','c','v',
    'b','n','m',',','.','/',0,'*',0,' ',0,0,0,0,0,0,
};

static const u8 ascii_table_shift[] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b','\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,'A','S',
    'D','F','G','H','J','K','L',':','"','~',0,'|','Z','X','C','V',
    'B','N','M','<','>','?',0,'*',0,' ',0,0,0,0,0,0,
};

static u32 editor_line_count(const editor_buffer_t *buf) {
    if (buf->size == 0) return 1;

    u32 count = 1;
    for (u32 i = 0; i < buf->size; i++) {
        if (buf->content[i] == '\n') {
            count++;
        }
    }
    return count;
}

static u32 editor_line_start(const editor_buffer_t *buf, u32 line_index) {
    u32 current_line = 0;
    u32 start = 0;

    for (u32 i = 0; i < buf->size; i++) {
        if (current_line == line_index) {
            return start;
        }
        if (buf->content[i] == '\n') {
            current_line++;
            start = i + 1;
        }
    }

    return start;
}

static u32 editor_line_end(const editor_buffer_t *buf, u32 line_index) {
    u32 start = editor_line_start(buf, line_index);
    u32 end = start;

    while (end < buf->size && buf->content[end] != '\n') {
        end++;
    }

    return end;
}

static void editor_move_cursor_left(editor_buffer_t *buf) {
    if (buf->cursor > 0) {
        buf->cursor--;
    }
}

static void editor_move_cursor_right(editor_buffer_t *buf) {
    if (buf->cursor < buf->size) {
        buf->cursor++;
    }
}

static void editor_move_vertical(editor_buffer_t *buf, int delta) {
    if (buf->size == 0) {
        return;
    }

    u32 current_line = 0;
    for (u32 i = 0; i < buf->cursor; i++) {
        if (buf->content[i] == '\n') {
            current_line++;
        }
    }

    int target_line = (int)current_line + delta;
    if (target_line < 0) {
        target_line = 0;
    }

    u32 total_lines = editor_line_count(buf);
    if ((u32)target_line >= total_lines) {
        target_line = (int)total_lines - 1;
    }

    u32 current_line_start = editor_line_start(buf, (u32)current_line);
    u32 current_col = buf->cursor - current_line_start;
    u32 target_line_start = editor_line_start(buf, (u32)target_line);
    u32 target_line_end = editor_line_end(buf, (u32)target_line);
    u32 target_col = current_col;
    u32 target_line_len = target_line_end - target_line_start;
    if (target_col > target_line_len) {
        target_col = target_line_len;
    }

    buf->cursor = target_line_start + target_col;
}

static void editor_put_text_at(int x, int y, const char *text, unsigned char color) {
    int cx = x;
    int cy = y;
    while (*text) {
        if (*text == '\n') {
            cx = x;
            cy++;
        } else {
            vga_write_cell(cx, cy, *text, color);
            cx++;
        }
        text++;
    }
}

static void editor_redraw(editor_buffer_t *buf, const char *filepath, u8 save_status) {
    vga_clear();
    vga_disable_hardware_cursor();

    editor_put_text_at(0, 0, "^X Exit | ^S Save | Arrows Move Cursor", 0x0F);
    editor_put_text_at(0, 1, "File: ", 0x0F);
    editor_put_text_at(6, 1, filepath, 0x0F);
    editor_put_text_at(0, 2, "=====================", 0x0F);

    if (save_status == 1) editor_put_text_at(0, 3, ">>> SAVING... <<<", 0x0F);
    else if (save_status == 2) editor_put_text_at(0, 3, ">>> SAVED! <<<", 0x0F);
    else if (save_status == 3) editor_put_text_at(0, 3, ">>> SAVE FAILED! <<<", 0x0F);

    int cursor_x = 0;
    int cursor_y = 5;

    if (buf->size == 0) {
        editor_put_text_at(0, cursor_y, "[Empty file - start typing]", 0x0F);
        cursor_x = 0;
        cursor_y += 1;
        vga_write_cell(0, 5, '|', 0x0C);
    } else {
        for (u32 i = 0; i < buf->size; i++) {
            char ch = buf->content[i];
            if (ch == '\n') {
                cursor_x = 0;
                cursor_y++;
                continue;
            }

            if (i == buf->cursor) {
                vga_write_cell(cursor_x, cursor_y, ch, 0x0F);
                vga_write_cell(cursor_x + 1, cursor_y, '|', 0x0C);
                cursor_x += 2;
                continue;
            }

            vga_write_cell(cursor_x, cursor_y, ch, 0x0F);
            cursor_x++;
        }

        if (buf->cursor == buf->size) {
            vga_write_cell(cursor_x, cursor_y, '|', 0x0C);
        }
    }

    editor_put_text_at(0, cursor_y + 1, "=====================", 0x0F);
}

static u8 vfs_write_file(const char *path, const u8 *data, u32 size) {
    /* Use the VFS wrapper so path normalization and error reporting are handled consistently. */
    u32 fd = vfs_open(path);
    if (fd == VFS_INVALID_FD) {
        /* If the file does not exist yet, try to create it before writing. */
        if (!vfs_create(path, 1)) {
            kprintf("[EDITOR] ERROR: Could not open or create file: %s\n", path);
            return 0;
        }
        fd = vfs_open(path);
        if (fd == VFS_INVALID_FD) {
            kprintf("[EDITOR] ERROR: Could not open file after creation: %s\n", path);
            return 0;
        }
    }

    u32 written = vfs_write(fd, data, size);
    if (written != size) {
        kprintf("[EDITOR] ERROR: write failed (expected %u, wrote %u bytes)\n", size, written);
    }

    vfs_close(fd);
    return (written == size) ? 1 : 0;
}

static void editor_handle_key(editor_buffer_t *buf, u8 scancode, u8 is_pressed, u8 extended,
                               u8 *save_flag, u8 *exit_flag, u8 *buffer_changed) {
    *buffer_changed = 0;
    if (!is_pressed) return;
    
    u8 raw = scancode & 0x7F;
    
    /* Ctrl+S (scancode 0x1F = 's') */
    if (keyboard_ctrl_pressed() && raw == 0x1F) {
        *save_flag = 1;
        return;
    }
    
    /* Ctrl+X (scancode 0x2D = 'x') */
    if (keyboard_ctrl_pressed() && raw == 0x2D) {
        *exit_flag = 1;
        return;
    }

    /* Esc is a fallback exit in case a Ctrl release event was missed. */
    if (raw == 0x01) {
        *exit_flag = 1;
        return;
    }
    
    /* If Ctrl was active but this was not a save/exit combo, discard the key. */
    if (keyboard_ctrl_pressed()) {
        return;
    }

    if (extended) {
        switch (raw) {
            case 0x4B:
                editor_move_cursor_left(buf);
                *buffer_changed = 1;
                return;
            case 0x4D:
                editor_move_cursor_right(buf);
                *buffer_changed = 1;
                return;
            case 0x48:
                editor_move_vertical(buf, -1);
                *buffer_changed = 1;
                return;
            case 0x50:
                editor_move_vertical(buf, 1);
                *buffer_changed = 1;
                return;
            default:
                return;
        }
    }
    
    if (raw >= sizeof(ascii_table)) return;
    
    u8 c = keyboard_shift_pressed() ? ascii_table_shift[raw] : ascii_table[raw];
    if (c == 0) return;
    
    if (c == '\b') {
        if (buf->cursor > 0) {
            for (u32 i = buf->cursor; i < buf->size; i++) {
                buf->content[i - 1] = buf->content[i];
            }
            buf->size--;
            buf->cursor--;
            *buffer_changed = 1;
        }
    } else if (c == '\n') {
        if (buf->size < EDITOR_BUFFER_SIZE - 1) {
            for (u32 i = buf->size; i > buf->cursor; i--) {
                buf->content[i] = buf->content[i - 1];
            }
            buf->content[buf->cursor] = '\n';
            buf->size++;
            buf->cursor++;
            *buffer_changed = 1;
        }
    } else if (c >= 32 && c < 127) {
        if (buf->size < EDITOR_BUFFER_SIZE - 1) {
            for (u32 i = buf->size; i > buf->cursor; i--) {
                buf->content[i] = buf->content[i - 1];
            }
            buf->content[buf->cursor] = c;
            buf->size++;
            buf->cursor++;
            *buffer_changed = 1;
        }
    }
}

static void editor_poll_keyboard(editor_buffer_t *buf, u8 *save_flag, u8 *exit_flag, u8 *buffer_changed) {
    u8 scancode;
    u8 is_pressed;
    u8 extended;

    if (!keyboard_read_event(&scancode, &is_pressed, &extended)) {
        return;
    }

    if (!is_pressed) {
        return;
    }

    editor_handle_key(buf, scancode, is_pressed, extended, save_flag, exit_flag, buffer_changed);
}

u8 shell_editor(const char *filepath) {
    if (!filepath || *filepath == 0) {
        kprintf("[EDITOR] No file specified\n");
        return 0;
    }

    editor_buffer_t buf = {0};
    u8 save_flag = 0, exit_flag = 0;
    u8 buffer_changed = 0;
    
    /* Load existing content */
    vfs_entry_t *entry = vfs_find(filepath);
    if (entry && !entry->is_dir && entry->size > 0) {
        u32 to_read = (entry->size < EDITOR_BUFFER_SIZE) ? entry->size : EDITOR_BUFFER_SIZE;
        vfs_read(filepath, buf.content, to_read);
        buf.size = to_read;
        buf.cursor = buf.size;
    }
    
    kprintf("[EDITOR] Opening %s\n", filepath);
    kprintf("[EDITOR] Press Ctrl+S to save, Ctrl+X to exit\n");
    keyboard_reset_state();
    vga_disable_hardware_cursor();
    
    /* Small delay to show message */
    for (volatile int i = 0; i < 200000; i++);
    
    editor_redraw(&buf, filepath, 0);

    while (!exit_flag) {
        editor_poll_keyboard(&buf, &save_flag, &exit_flag, &buffer_changed);

        if (buffer_changed) {
            editor_redraw(&buf, filepath, 0);
            buffer_changed = 0;
        }
        
        if (save_flag) {
            save_flag = 0;
            editor_redraw(&buf, filepath, 1);
            
            if (vfs_write_file(filepath, (u8*)buf.content, buf.size)) {
                /* Force filesystem sync to ensure data is written to disk */
                vfs_fsync();
                editor_redraw(&buf, filepath, 2);
            } else {
                editor_redraw(&buf, filepath, 3);
            }
        }
        
        /* Small delay to avoid CPU spinning */
        for (volatile int i = 0; i < 100; i++);
    }
    
    keyboard_reset_state();
    vga_enable_hardware_cursor();
    vga_clear();
    kprintf("[EDITOR] Exited\n");
    return 1;
}
