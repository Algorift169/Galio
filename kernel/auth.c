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
#include "keyboard.h"
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


/* Unified account data file path */
static const char *auth_data_path = "./var/account/.udata.galio";
static const char *auth_data_dir = "./var/account";

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

static char *auth_find_key_line(char *buffer, const char *key) {
    char *cursor = buffer;
    u32 key_len = strlen(key);

    while (*cursor != 0) {
        if (strncmp(cursor, key, key_len) == 0) {
            return cursor;
        }

        if (*cursor == '\n' || *cursor == '\r') {
            cursor++;
            continue;
        }

        cursor++;
    }

    return NULL;
}

static char *auth_line_end(char *text) {
    char *cursor = text;
    while (*cursor != 0 && *cursor != '\n' && *cursor != '\r') {
        cursor++;
    }
    return cursor;
}

static i32 auth_save_to_disk(void) {
    if (!vfs_core_is_disk_mode()) return -1;

    if (!vfs_mkdir("./var", 1)) return -1;
    if (!vfs_mkdir(auth_data_dir, 1)) return -1;
    if (!vfs_mkdir("./root", 1)) return -1;
    if (!vfs_mkdir("./root/home", 1)) return -1;

    /* Create or overwrite unified account data file */
    if (!vfs_create(auth_data_path, 1)) return -1;

    u32 data_fd = vfs_open(auth_data_path);
    if (data_fd == VFS_INVALID_FD) return -1;

    char payload[256];
    payload[0] = 0;

    strncat(payload, "usr = ", sizeof(payload) - 1);
    strncat(payload, kernel_auth.username, sizeof(payload) - strlen(payload) - 1);
    strncat(payload, "\n", sizeof(payload) - strlen(payload) - 1);
    strncat(payload, "pass = ", sizeof(payload) - 1);
    strncat(payload, kernel_auth.password, sizeof(payload) - strlen(payload) - 1);
    strncat(payload, "\n", sizeof(payload) - strlen(payload) - 1);

    u32 payload_len = strlen(payload);
    u32 written = vfs_write(data_fd, payload, payload_len);
    vfs_close(data_fd);
    if (written != payload_len) return -1;

    if (!vfs_fsync()) return -1;
    if (auth_ensure_home_directory() != 0) return -1;
    return 0;
}

static i32 auth_load_from_disk(void) {
    if (!vfs_core_is_disk_mode()) return -1;

    if (ext2_find_inode(auth_data_path) == 0) {
        return 0;
    }

    char data_buffer[256];
    u32 data_len = vfs_read(auth_data_path, data_buffer, sizeof(data_buffer) - 1);
    if (data_len == 0) return 0;

    data_buffer[data_len] = 0;

    char username[INPUT_BUFFER_SIZE];
    char password[INPUT_BUFFER_SIZE];
    username[0] = 0;
    password[0] = 0;

    char *usr_line = auth_find_key_line(data_buffer, "usr = ");
    char *pass_line = auth_find_key_line(data_buffer, "pass = ");

    if (usr_line && pass_line) {
        char *usr_value = usr_line + strlen("usr = ");
        char *usr_end = auth_line_end(usr_value);
        u32 user_len = (u32)(usr_end - usr_value);
        if (user_len >= sizeof(username)) return 0;
        strncpy(username, usr_value, user_len);
        username[user_len] = 0;

        char *pass_value = pass_line + strlen("pass = ");
        char *pass_end = auth_line_end(pass_value);
        u32 pass_len = (u32)(pass_end - pass_value);
        if (pass_len >= sizeof(password)) return 0;
        strncpy(password, pass_value, pass_len);
        password[pass_len] = 0;
    } else {
        /* Legacy format: username:password */
        char *delimiter = NULL;
        char *cursor = data_buffer;
        while (*cursor != 0) {
            if (*cursor == ':') {
                delimiter = cursor;
                break;
            }
            cursor++;
        }
        if (!delimiter) return 0;

        u32 user_len = (u32)(delimiter - data_buffer);
        if (user_len >= sizeof(username)) return 0;
        strncpy(username, data_buffer, user_len);
        username[user_len] = 0;

        const char *pass_start = delimiter + 1;
        u32 pass_len = data_len - user_len - 1;
        if (pass_len >= sizeof(password)) return 0;
        strncpy(password, pass_start, pass_len);
        password[pass_len] = 0;
    }

    if (username[0] == 0 || password[0] == 0) {
        return 0;
    }

    strncpy(kernel_auth.username, username, sizeof(kernel_auth.username) - 1);
    kernel_auth.username[sizeof(kernel_auth.username) - 1] = 0;
    strncpy(kernel_auth.password, password, sizeof(kernel_auth.password) - 1);
    kernel_auth.password[sizeof(kernel_auth.password) - 1] = 0;

    kernel_auth.registered = 1;
    kernel_auth.authenticated = 0;
    auth_set_session_uid();
    auth_ensure_home_directory();
    return 1;
}

static u8 read_char(void) {
    u8 scancode;
    u8 is_pressed;
    u8 extended;

    while (1) {
        if (!keyboard_read_event(&scancode, &is_pressed, &extended)) {
            for (volatile int i = 0; i < 100; i++);
            continue;
        }

        if (!is_pressed) {
            continue;
        }

        if (extended) {
            continue;
        }

        u8 c = scancode_to_ascii(scancode);
        if (c == 0 || c == '\t') {
            continue;
        }

        return c;
    }
}

static void read_line(char *buffer, u32 max_len, u8 echo) {
    if (!buffer || max_len == 0) {
        return;
    }

    keyboard_clear_pending_input();

    u32 len = 0;
    for (;;) {
        u8 c = read_char();

        /* Enter: finish line */
        if (c == '\n') {
            buffer[len] = 0;
            kprintf("\n");
            break;
        }

        /* Backspace: remove previous character if any */
        if (c == '\b') {
            if (len > 0) {
                len--;
                kprintf("\b \b");
            }
            continue;
        }

        /* Printable characters */
        if (c >= 32 && c < 127) {
            if (len < max_len - 1) {
                buffer[len++] = c;
                if (echo) {
                    vga_putch(c);
                } else {
                    vga_putch('*');  /* Mask password */
                }
            } else {
                /* Buffer full: ignore additional printable characters but allow backspace/enter */
                /* Optional: give user feedback (beep) by writing a space then backspace */
                vga_putch('\a' /* BEL */);
            }
        }
    }
    /* Ensure NUL termination */
    if (max_len > 0) buffer[(max_len - 1) < len ? (max_len - 1) : len] = 0;
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
    vga_set_color(0x0A);
    kprintf("\n");
    kprintf("╔══════════════════════════════════════════════════════════════╗\n");
    kprintf("║           Galio Kernel Registration                          ║\n");
    kprintf("║          Create a kernel account for galio                   ║\n");
    kprintf("╚══════════════════════════════════════════════════════════════╝\n");
    kprintf("\n");
    vga_set_color(0x0A);
}

void auth_bootstrap(void) {
    kernel_auth.registered = 0;
    kernel_auth.authenticated = 0;

    char username[INPUT_BUFFER_SIZE];
    char password[INPUT_BUFFER_SIZE];
    char confirm[INPUT_BUFFER_SIZE];

    if (!vfs_core_is_disk_mode()) {
        vga_set_color(0x0C);
        kprintf("[AUTH] Disk-backed filesystem unavailable, saved credentials cannot be loaded.\n");
        vga_set_color(0x0A);
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
                vga_set_color(0x0A);
                kprintf("\n[AUTH] Authentication successful.\n\n");
                return;
            }

            vga_set_color(0x0C);
            kprintf("\n[AUTH] Invalid password. Try again.\n\n");
            vga_set_color(0x0A);
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
            vga_set_color(0x0C);
            kprintf("[AUTH] Username and password cannot be empty. Try again.\n\n");
            vga_set_color(0x0A);
            continue;
        }
        if (strcmp(password, confirm) != 0) {
            vga_set_color(0x0C);
            kprintf("[AUTH] Passwords do not match. Try again.\n\n");
            vga_set_color(0x0A);
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
            vga_set_color(0x0A);
            kprintf("[AUTH] Credentials saved to disk.\n");
        } else {
            vga_set_color(0x0C);
            kprintf("[AUTH] Warning: could not persist credentials to disk.\n");
            vga_set_color(0x0A);
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

/* Change password - requires old password verification */
i32 auth_change_password(const char *old_password, const char *new_password) {
    if (!kernel_auth.authenticated) {
        return -1;  /* Not authenticated */
    }
    if (!old_password || !new_password) {
        return -1;
    }
    if (old_password[0] == 0 || new_password[0] == 0) {
        return -1;
    }

    /* Verify old password */
    if (!auth_verify_password(kernel_auth.username, old_password)) {
        return -2;  /* Password verification failed */
    }

    /* Update password */
    strncpy(kernel_auth.password, new_password, sizeof(kernel_auth.password) - 1);
    kernel_auth.password[sizeof(kernel_auth.password) - 1] = 0;

    /* Save to disk */
    if (auth_save_to_disk() != 0) {
        return -3;  /* Disk write failed */
    }

    return 0;  /* Success */
}

/* Change username - requires root privilege (rex) */
i32 auth_change_username(const char *new_username, u8 require_root) {
    if (!kernel_auth.authenticated) {
        return -1;  /* Not authenticated */
    }
    if (require_root && kernel_auth.uid != UID_ROOT) {
        return -2;  /* Root privilege required */
    }
    if (!new_username || new_username[0] == 0) {
        return -3;  /* Invalid username */
    }

    /* Verify new username is not too long */
    if (strlen(new_username) >= sizeof(kernel_auth.username)) {
        return -4;  /* Username too long */
    }

    /* Update username */
    strncpy(kernel_auth.username, new_username, sizeof(kernel_auth.username) - 1);
    kernel_auth.username[sizeof(kernel_auth.username) - 1] = 0;
    auth_set_session_uid();
    session_login(kernel_auth.username, kernel_auth.uid);

    /* Save to disk */
    if (auth_save_to_disk() != 0) {
        return -5;  /* Disk write failed */
    }

    return 0;  /* Success */
}
