/* auth.c - User authentication system */
#include "auth.h"
#include "ext2.h"
#include "vfs.h"
#include "vfs_core.h"
#include "vga.h"
#include "kprintf.h"
#include "string.h"
#include "cpu.h"
#include "process/process.h"
#include <stddef.h>

user_session_t kernel_auth = {0};

static void auth_set_session_uid(void) {
    if (strcmp(kernel_auth.username, "root") == 0) {
        kernel_auth.uid = UID_ROOT;
        kernel_auth.gid = UID_ROOT;
    } else {
        kernel_auth.uid = UID_USER;
        kernel_auth.gid = UID_USER;
    }
}

void session_login(const char *username, u32 uid) {
    if (username) {
        strncpy(kernel_auth.username, username, sizeof(kernel_auth.username) - 1);
        kernel_auth.username[sizeof(kernel_auth.username) - 1] = 0;
    }
    kernel_auth.uid = uid;
    kernel_auth.gid = (uid == UID_ROOT) ? UID_ROOT : UID_USER;
    kernel_auth.registered = 1;
    kernel_auth.authenticated = 1;
    kernel_auth.authorized = 0; /* Require explicit rex authorization for privileged commands */
}

void session_logout(void) {
    kernel_auth.authenticated = 0;
    kernel_auth.authorized = 0;
}

user_session_t *session_current(void) {
    return &kernel_auth;
}


static const char *auth_user_path = "./root/usr/usr.txt";
static const char *auth_pass_path = "./root/usr/pass.txt";
static const char *auth_directory_path = "./root/usr";

static i32 auth_ensure_home_directory(void) {
    if (!vfs_core_is_disk_mode()) return -1;

    if (!vfs_mkdir("./root", 1)) return -1;
    if (!vfs_mkdir("./root/home", 1)) return -1;

    if (kernel_auth.username[0] == 0) return 0;

    char home_path[64];
    u32 base_len = strlen("./root/home/");
    if (base_len + strlen(kernel_auth.username) >= sizeof(home_path)) return -1;
    strncpy(home_path, "./root/home/", sizeof(home_path) - 1);
    home_path[sizeof(home_path) - 1] = 0;
    strncat(home_path, kernel_auth.username, sizeof(home_path) - base_len - 1);

    if (!vfs_mkdir(home_path, 1)) return -1;
    return 0;
}

static i32 auth_save_to_disk(void) {
    if (!vfs_core_is_disk_mode()) return -1;

    if (!vfs_mkdir("./root", 1)) return -1;
    if (!vfs_mkdir(auth_directory_path, 1)) return -1;
    if (!vfs_mkdir("./root/home", 1)) return -1;

    if (!vfs_create(auth_user_path, 1)) return -1;
    if (!vfs_create(auth_pass_path, 1)) return -1;

    u32 user_fd = vfs_open(auth_user_path);
    if (user_fd == VFS_INVALID_FD) return -1;
    u32 pass_fd = vfs_open(auth_pass_path);
    if (pass_fd == VFS_INVALID_FD) {
        vfs_close(user_fd);
        return -1;
    }

    u32 username_len = strlen(kernel_auth.username);
    u32 written = vfs_write(user_fd, kernel_auth.username, username_len);
    vfs_close(user_fd);
    if (written != username_len) {
        vfs_close(pass_fd);
        return -1;
    }

    u32 password_len = strlen(kernel_auth.password);
    written = vfs_write(pass_fd, kernel_auth.password, password_len);
    vfs_close(pass_fd);
    if (written != password_len) return -1;

    if (!vfs_fsync()) return -1;
    if (auth_ensure_home_directory() != 0) return -1;
    return 0;
}

static i32 auth_load_from_disk(void) {
    if (!vfs_core_is_disk_mode()) return -1;

    if (ext2_find_inode(auth_user_path) == 0 || ext2_find_inode(auth_pass_path) == 0) {
        return 0;
    }

    char user_buffer[sizeof(kernel_auth.username)];
    char pass_buffer[sizeof(kernel_auth.password)];

    u32 user_len = vfs_read(auth_user_path, user_buffer, sizeof(user_buffer) - 1);
    u32 pass_len = vfs_read(auth_pass_path, pass_buffer, sizeof(pass_buffer) - 1);
    if (user_len == 0 || pass_len == 0) return 0;

    user_buffer[user_len] = 0;
    pass_buffer[pass_len] = 0;

    strncpy(kernel_auth.username, user_buffer, sizeof(kernel_auth.username) - 1);
    kernel_auth.username[sizeof(kernel_auth.username) - 1] = 0;
    strncpy(kernel_auth.password, pass_buffer, sizeof(kernel_auth.password) - 1);
    kernel_auth.password[sizeof(kernel_auth.password) - 1] = 0;
    kernel_auth.registered = 1;
    kernel_auth.authenticated = 0;
    auth_set_session_uid();
    auth_ensure_home_directory();
    return 1;
}

static u8 read_char(void) {
    while (1) {
        u8 status = inb(0x64);
        if (status & 0x01) {
            /* Check if this is mouse data (bit 5 of status port indicates auxiliary device buffer) */
            if (status & 0x20) {
                /* Mouse data - discard it without processing */
                inb(0x60);
                continue;
            }

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

    /* Clear boot logs and prepare the screen for authentication/registration */
    vga_clear();

    if (loaded == 1) {
        while (1) {
            kprintf("Password for %s: ", kernel_auth.username);
            read_line(password, INPUT_BUFFER_SIZE, 0);

            if (auth_verify_password(kernel_auth.username, password)) {
                auth_set_session_uid();
                session_login(kernel_auth.username, kernel_auth.uid);
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
        auth_set_session_uid();
        session_login(kernel_auth.username, kernel_auth.uid);

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
    return kernel_auth.authorized;
}

void auth_authorize(void) {
    kernel_auth.authorized = 1;
}