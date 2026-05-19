/* auth.c - User authentication system */
#include "auth.h"
#include "ext2.h"
#include "vfs_core.h"
#include "vga.h"
#include "kprintf.h"
#include "string.h"
#include "cpu.h"
#include <stddef.h>

user_session_t kernel_auth = {0};

static const char *auth_credentials_path = "/etc/galio_auth";
static const char *auth_directory_path = "/etc";

static i32 auth_save_to_disk(void) {
    if (!vfs_core_is_disk_mode()) return -1;

    if (ext2_find_inode(auth_directory_path) == 0) {
        if (ext2_create_directory(auth_directory_path, 0x41ED) < 0) {
            return -1;
        }
    }

    u32 inode_num = ext2_find_inode(auth_credentials_path);
    if (inode_num == 0) {
        i32 created = ext2_create_file(auth_credentials_path, 0x81A4);
        if (created < 0) return -1;
        inode_num = (u32)created;
    }

    char buf[64];
    u32 len = 0;
    while (kernel_auth.username[len] && len < sizeof(buf) - 2) {
        buf[len] = kernel_auth.username[len];
        len++;
    }
    if (len < sizeof(buf) - 1) {
        buf[len++] = ':';
    }
    u32 idx = 0;
    while (kernel_auth.password[idx] && len < sizeof(buf) - 1 && idx < sizeof(kernel_auth.password) - 1) {
        buf[len++] = kernel_auth.password[idx++];
    }
    buf[len] = 0;

    if (ext2_write_data(inode_num, buf, len) < 0) {
        return -1;
    }

    /* Ensure credentials are persisted */
    extern void memory_disk_fsync(void);
    memory_disk_fsync();

    return 0;
}

static i32 auth_load_from_disk(void) {
    if (!vfs_core_is_disk_mode()) return -1;

    u32 inode_num = ext2_find_inode(auth_credentials_path);
    if (inode_num == 0) return 0;

    ext2_inode_t inode;
    if (ext2_read_inode(inode_num, &inode) != 0) return -1;
    if (inode.block[0] == 0 || inode.size == 0) return -1;

    u8 buffer[4096];
    if (ext2_read_block(inode.block[0], buffer) != 0) return -1;
    u32 read_len = inode.size;
    if (read_len >= sizeof(buffer)) read_len = sizeof(buffer) - 1;
    buffer[read_len] = 0;

    char *sep = NULL;
    for (u32 i = 0; i < read_len; i++) {
        if (buffer[i] == ':') {
            sep = (char *)&buffer[i];
            break;
        }
    }
    if (!sep) return -1;

    u32 username_len = sep - (char *)buffer;
    u32 password_len = read_len - username_len - 1;
    if (username_len == 0 || password_len == 0) return -1;

    if (username_len >= sizeof(kernel_auth.username)) username_len = sizeof(kernel_auth.username) - 1;
    if (password_len >= sizeof(kernel_auth.password)) password_len = sizeof(kernel_auth.password) - 1;

    memcpy(kernel_auth.username, buffer, username_len);
    kernel_auth.username[username_len] = 0;
    memcpy(kernel_auth.password, sep + 1, password_len);
    kernel_auth.password[password_len] = 0;
    kernel_auth.registered = 1;
    kernel_auth.authenticated = 0;
    return 1;
}

static u8 read_char(void) {
    while (1) {
        u8 status = inb(0x64);
        if (status & 0x01) {
            u8 scancode = inb(0x60);
            u8 is_pressed = !(scancode & 0x80);
            u8 raw_scancode = scancode & 0x7F;

            static const u8 ascii_table[] = {
                0,  27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
                'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's',
                'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
                'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
            };
            static const u8 ascii_table_shift[] = {
                0,  27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
                'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0, 'A', 'S',
                'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
                'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
            };
            static u8 shift_pressed = 0;

            if (raw_scancode == 0x2A || raw_scancode == 0x36) {
                shift_pressed = is_pressed;
                continue;
            }

            if (!is_pressed) {
                continue;
            }

            if (raw_scancode < sizeof(ascii_table)) {
                u8 c = shift_pressed ? ascii_table_shift[raw_scancode] : ascii_table[raw_scancode];
                if (c != 0 && c != '\t') {
                    return c;
                }
            }
        }
        for (volatile int i = 0; i < 100; i++);
    }
}

static void read_line(char *buffer, u32 max_len, u8 echo) {
    u32 len = 0;
    while (len < max_len - 1) {
        u8 c = read_char();

        if (c == '\n') {
            buffer[len] = 0;
            kprintf("\n");
            break;
        } else if (c == '\b') {
            if (len > 0) {
                len--;
                kprintf("\b \b");
            }
        } else if (c >= 32 && c < 127) {
            buffer[len++] = c;
            if (echo) {
                vga_putch(c);
            } else {
                vga_putch('*');  /* Mask password */
            }
        }
    }
    buffer[max_len - 1] = 0;
}

/* Simple password verification - using boot-time registration or hardcoded defaults */
u8 auth_verify_password(const char *username, const char *password) {
    if (kernel_auth.registered) {
        return strcmp(username, kernel_auth.username) == 0 && strcmp(password, kernel_auth.password) == 0;
    }

    /* Default credentials for testing - change in production */
    if (strcmp(username, "galio") == 0 && strcmp(password, "galio") == 0) {
        return 1;
    }
    if (strcmp(username, "root") == 0 && strcmp(password, "root") == 0) {
        return 1;
    }
    if (strcmp(username, "admin") == 0 && strcmp(password, "admin123") == 0) {
        return 1;
    }
    return 0;
}

void auth_show_login_prompt(void) {
    kprintf("\n");
    kprintf("╔══════════════════════════════════════════════════════════════╗\n");
    kprintf("║           Galio Kernel Registration                          ║\n");
    kprintf("║          Create a kernel account for galio                   ║\n");
    kprintf("╚══════════════════════════════════════════════════════════════╝\n");
    kprintf("\n");
}

void auth_bootstrap(void) {
    kernel_auth.registered = 0;
    kernel_auth.authenticated = 0;

    char username[INPUT_BUFFER_SIZE];
    char password[INPUT_BUFFER_SIZE];
    char confirm[INPUT_BUFFER_SIZE];

    if (!vfs_core_is_disk_mode()) {
        kprintf("[AUTH] Disk-backed filesystem unavailable, saved credentials cannot be loaded.\n");
    }

    i32 loaded = auth_load_from_disk();

    if (loaded == 1) {
        while (1) {
            kprintf("Password for %s: ", kernel_auth.username);
            read_line(password, INPUT_BUFFER_SIZE, 0);

            if (auth_verify_password(kernel_auth.username, password)) {
                auth_authorize();
                kprintf("\n[AUTH] Authentication successful.\n\n");
                return;
            }

            kprintf("\n[AUTH] Invalid password. Try again.\n\n");
        }
    }

    auth_show_login_prompt();

    while (1) {
        kprintf("Username: ");
        read_line(username, INPUT_BUFFER_SIZE, 1);

        kprintf("Password: ");
        read_line(password, INPUT_BUFFER_SIZE, 0);

        kprintf("Confirm Password: ");
        read_line(confirm, INPUT_BUFFER_SIZE, 0);

        if (username[0] == 0 || password[0] == 0) {
            kprintf("[AUTH] Username and password cannot be empty. Try again.\n\n");
            continue;
        }
        if (strcmp(password, confirm) != 0) {
            kprintf("[AUTH] Passwords do not match. Try again.\n\n");
            continue;
        }

        strncpy(kernel_auth.username, username, sizeof(kernel_auth.username) - 1);
        kernel_auth.username[sizeof(kernel_auth.username) - 1] = 0;
        strncpy(kernel_auth.password, password, sizeof(kernel_auth.password) - 1);
        kernel_auth.password[sizeof(kernel_auth.password) - 1] = 0;
        kernel_auth.registered = 1;
        kernel_auth.authenticated = 0;

        if (auth_save_to_disk() == 0) {
            kprintf("[AUTH] Credentials saved to disk.\n");
        } else {
            kprintf("[AUTH] Warning: could not persist credentials to disk.\n");
        }

        kprintf("\n[AUTH] Kernel account registered for user '%s'.\n", kernel_auth.username);
        kprintf("[AUTH] Use 'rex' in the shell to run privileged commands.\n\n");
        return;
    }
}

u8 auth_prompt_password(const char *prompt, char *password, u32 max_len) {
    kprintf("%s", prompt);
    read_line(password, max_len, 0);
    return password[0] != 0;
}

u8 auth_is_authorized(void) {
    return kernel_auth.authenticated;
}

void auth_authorize(void) {
    kernel_auth.authenticated = 1;
}