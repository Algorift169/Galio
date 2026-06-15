#include "show.h"
#include "kprintf.h"
#include "string.h"
#include "path.h"
#include "vfs.h"

#define SHOW_BUFFER_SIZE 4096
#define BOX_WIDTH 50

static void safe_strcat(char *dest, const char *src, u32 max_len) {
    u32 dest_len = strlen(dest);
    u32 copy_len = max_len - dest_len - 1;
    if (copy_len > 0) {
        strncat(dest, src, copy_len);
        dest[max_len - 1] = 0;
    }
}

static void build_filepath(const char *args, const char *current_dir, char *out_path) {
    char filename[256];
    char target_dir[256];

    strncpy(filename, args, sizeof(filename) - 1);
    filename[sizeof(filename) - 1] = 0;
    
    char *p = filename;
    while (*p == ' ') p++;
    
    if (*p == 0 || strcmp(p, "show") == 0) {
        out_path[0] = 0;
        return;
    }

    path_resolve(current_dir, p, out_path, 256);
}

static void print_box_top(const char *filename) {
    u32 name_len = strlen(filename);
    u32 padding;
    
    kprintf("\n+--------------------------------------------------+\n");
    kprintf("|");
    
    /* Center the filename */
    if (name_len > BOX_WIDTH - 4) {
        kprintf(" %.30s... ", filename);
    } else {
        padding = (BOX_WIDTH - name_len - 2) / 2;
        for (u32 i = 0; i < padding; i++) kprintf(" ");
        kprintf("[ %s ]", filename);
        for (u32 i = padding + name_len + 4; i < BOX_WIDTH; i++) kprintf(" ");
    }
    
    kprintf("|\n");
    kprintf("+--------------------------------------------------+\n");
}

static void print_box_bottom(void) {
    kprintf("+--------------------------------------------------+\n");
}

u8 shell_show_command(const char *args, const char *current_dir) {
    if (!args || *args == 0) {
        kprintf("show: missing file operand\n");
        kprintf("Try 'show <filename>' to display file contents\n");
        return 0;
    }

    char fullpath[256];
    build_filepath(args, current_dir, fullpath);
    
    if (fullpath[0] == 0) {
        kprintf("show: no file specified\n");
        return 0;
    }

    vfs_entry_t *entry = vfs_find(fullpath);
    if (!entry) {
        kprintf("show: %s: No such file or directory\n", fullpath);
        return 0;
    }

    if (entry->is_dir) {
        kprintf("show: %s: Is a directory\n", fullpath);
        return 0;
    }

    if (entry->size == 0) {
        print_box_top(fullpath);
        kprintf("|                                                                      |\n");
        kprintf("|                    [EMPTY FILE]               |\n");
        kprintf("|                                                                      |\n");
        print_box_bottom();
        return 1;
    }

    char buffer[SHOW_BUFFER_SIZE];
    u32 bytes_read = vfs_read(fullpath, buffer, SHOW_BUFFER_SIZE - 1);
    buffer[bytes_read] = 0;

    print_box_top(fullpath);
    
    /* Print file content */
    u32 i = 0;
    while (i < bytes_read) {
        kprintf("| ");
        
        /* Print up to BOX_WIDTH - 4 characters per line */
        u32 chars_printed = 0;
        while (i < bytes_read && chars_printed < BOX_WIDTH - 4) {
            char c = buffer[i];
            if (c == '\n') {
                i++;
                break;
            }
            if (c == '\t') {
                kprintf("    ");
                chars_printed += 4;
            } else {
                kprintf("%c", c);
                chars_printed++;
            }
            i++;
        }
        
        /* Pad the rest of the line */
        for (u32 j = chars_printed; j < BOX_WIDTH - 4; j++) {
            kprintf(" ");
        }
        kprintf(" |\n");
    }
    
    print_box_bottom();
    kprintf("\n");

    return 1;
}