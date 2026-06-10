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

static void editor_display(editor_buffer_t *buf, const char *filepath, u8 save_status) {
    vga_clear();
    kprintf("^X Exit | ^S Save | Galio Text Editor\n");
    kprintf("File: %s\n", filepath);
    kprintf("-------------------------------------------\n");
    
    if (save_status == 1) kprintf(">>> SAVING... <<<\n");
    else if (save_status == 2) kprintf(">>> SAVED! <<<\n");
    else if (save_status == 3) kprintf(">>> SAVE FAILED! <<<\n");
    
    kprintf("\n");
    
    /* Display buffer content */
    if (buf->size == 0) {
        kprintf("[Empty file - start typing]\n");
    } else {
        for (u32 i = 0; i < buf->size; i++) {
            if (buf->content[i] == '\n') kprintf("\n");
            else kprintf("%c", buf->content[i]);
        }
    }
    
    kprintf("\n\n-------------------------------------------\n");
    kprintf("Cursor position: %u chars\n", buf->size);
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

static void editor_handle_key(editor_buffer_t *buf, u8 scancode, u8 is_pressed, 
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
    
    if (raw >= sizeof(ascii_table)) return;
    
    u8 c = keyboard_shift_pressed() ? ascii_table_shift[raw] : ascii_table[raw];
    if (c == 0) return;
    
    if (c == '\b') {
        if (buf->size > 0) {
            buf->size--;
            kprintf("\b \b");
            *buffer_changed = 1;
        }
    } else if (c == '\n') {
        if (buf->size < EDITOR_BUFFER_SIZE - 1) {
            buf->content[buf->size++] = '\n';
            kprintf("\n");
            *buffer_changed = 1;
        }
    } else if (c >= 32 && c < 127) {
        if (buf->size < EDITOR_BUFFER_SIZE - 1) {
            buf->content[buf->size++] = c;
            kprintf("%c", c);
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

    if (extended) {
        return;
    }

    editor_handle_key(buf, scancode, is_pressed, save_flag, exit_flag, buffer_changed);
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
    }
    
    kprintf("[EDITOR] Opening %s\n", filepath);
    kprintf("[EDITOR] Press Ctrl+S to save, Ctrl+X to exit\n");
    keyboard_reset_state();
    
    /* Small delay to show message */
    for (volatile int i = 0; i < 200000; i++);
    
    /* Display header and initial content */
    vga_clear();
    kprintf("_____________________________________________________________\n");
    kprintf("                                                             \n");
    kprintf("                     Galio Text Editor                       \n");
    kprintf("                                                             \n");
    kprintf("                   [File: %s]                                \n", filepath);
    kprintf("_____________________________________________________________\n");
    kprintf("cntrl +X Exit | ^S Save | (Make sure to save before exiting!)\n");
    kprintf("-------------------------------------------------------------\n");
    
    if (buf.size == 0) {
        kprintf("[Empty file - start typing]\n");
        kprintf("_____________________________________________________________\n");
    } else {
        for (u32 i = 0; i < buf.size; i++) {
            if (buf.content[i] == '\n') kprintf("\n");
            else kprintf("%c", buf.content[i]);
        }
    }
    
    //kprintf("_____________________________________________________________\n");
    //kprintf("Cursor position: %u chars\n", buf.size);

    while (!exit_flag) {
        editor_poll_keyboard(&buf, &save_flag, &exit_flag, &buffer_changed);
        
        if (save_flag) {
            save_flag = 0;
            kprintf("\n>>> SAVING... <<<\n");
            
            if (vfs_write_file(filepath, (u8*)buf.content, buf.size)) {
                /* Force filesystem sync to ensure data is written to disk */
                vfs_fsync();
                kprintf(">>> SAVED! <<<\n");
            } else {
                kprintf(">>> SAVE FAILED! <<<\n");
            }
        }
        
        /* Small delay to avoid CPU spinning */
        for (volatile int i = 0; i < 100; i++);
    }
    
    keyboard_reset_state();
    vga_clear();
    kprintf("[EDITOR] Exited\n");
    return 1;
}
