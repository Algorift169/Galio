/* shell.c - Interactive kernel shell (POLLING MODE, NO IRQs) */
#include "shell.h"
#include "vga.h"
#include "kprintf.h"
#include "string.h"
#include "keyboard.h"
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <limits.h>
#include "arch/x86/cpu.h"
#include "vfs.h"
#include "auth.h"
#include "path.h"
#include "display/display.h"
#include "mouse/mouse.h"
#include "new.h"
#include "file.h"
#include "write.h"
#include "show.h"
#include "recycle.h"
#include "clean.h"
#include "delete.h"
#include "tree.h"
#include "net.h"
#include "pkg.h"
#include "editor.h"

u8 shell_net_command(const char *args, const char *current_dir);
u8 shell_pkg_command(const char *args, const char *current_dir);

static char *shell_strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    if (c == 0) return (char *)s;
    return NULL;
}

static int shell_atoi(const char *s) {
    int v = 0;
    int sign = 1;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') { s++; }
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (*s - '0');
        s++;
    }
    return v * sign;
}

#define SHELL_BUFFER_SIZE 256
#define HISTORY_SIZE 10
#define HISTORY_BUFFER_SIZE 256
#define DIR_HISTORY_SIZE 32
#define DIR_PATH_SIZE 256
#define ROOT_DIR "."
#define HOME_DIR "./usr/home"

/*
 * Shell color helpers — VGA attribute bytes (bg nibble | fg nibble):
 *   0x0A = light green on black  → command prompt / success labels
 *   0x0C = light red   on black  → error messages
 *   0x0E = yellow      on black  → normal output
 *   0x0F = white       on black  → default (user typing)
 */
#define SHELL_COLOR_CMD()    vga_set_color(0x0A)
#define SHELL_COLOR_ERR()    vga_set_color(0x0C)
#define SHELL_COLOR_OUT()    vga_set_color(0x0E)
#define SHELL_COLOR_RESET()  vga_set_color(0x0F)

/* ASCII lookup table for scancodes */
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

typedef struct {
    char buffer[SHELL_BUFFER_SIZE];
    u32 len;
} shell_input_t;

typedef struct {
    char history[HISTORY_SIZE][HISTORY_BUFFER_SIZE];
    u32 count;
    u32 head;
    u32 index;
} shell_history_t;

typedef struct {
    char stack[DIR_HISTORY_SIZE][DIR_PATH_SIZE];
    u32 sp;
} dir_history_t;

static shell_input_t input;
static shell_history_t history = {0};
static dir_history_t dir_history = {0};
static char current_dir[256] = HOME_DIR;
static const char shell_hostname[] = "galio";
static u8 shell_should_exit = 0;
static int shell_cursor_x = 0;
static int shell_cursor_y = 0;
static unsigned short shell_cursor_saved_cell = 0;
static u8 shell_cursor_drawn = 0;

static const char *shell_display_dir(const char *path, char *out, u32 out_size);
static u8 shell_is_direct_root_child(const char *path);

static void shell_cursor_restore(void) {
    if (!shell_cursor_drawn) return;
    char saved_char = (char)(shell_cursor_saved_cell & 0xFF);
    unsigned char saved_color = (unsigned char)(shell_cursor_saved_cell >> 8);
    /* Restore the original cell and re-enable hardware cursor */
    vga_write_cell(shell_cursor_x, shell_cursor_y, saved_char, saved_color);
    vga_enable_hardware_cursor();
    shell_cursor_drawn = 0;
}

static void shell_cursor_draw(void) {
    if (shell_cursor_drawn) return;
    vga_get_hardware_cursor(&shell_cursor_x, &shell_cursor_y);
    shell_cursor_saved_cell = vga_read_cell(shell_cursor_x, shell_cursor_y);
    char saved_char = (char)(shell_cursor_saved_cell & 0xFF);
    unsigned char saved_color = (unsigned char)(shell_cursor_saved_cell >> 8);
    unsigned char fg = saved_color & 0x0F;
    unsigned char bg = (saved_color >> 4) & 0x0F;
    unsigned char block_color = (unsigned char)((fg << 4) | bg);
    if ((block_color & 0x0F) == ((block_color >> 4) & 0x0F)) {
        block_color = 0x70;
    }
    vga_disable_hardware_cursor();
    vga_write_cell(shell_cursor_x, shell_cursor_y, saved_char, block_color);
    shell_cursor_drawn = 1;
}

static void shell_cursor_reset(void) {
    shell_cursor_restore();
    shell_cursor_draw();
}

static void shell_print_prompt(void) {
    shell_cursor_restore();
    const char *host = shell_hostname;
    char display_dir[DIR_PATH_SIZE];
    const char *dir = shell_display_dir(current_dir, display_dir, sizeof(display_dir));
    if (session_current() && session_current()->registered && session_current()->username[0]) {
        host = session_current()->username;
    }
    SHELL_COLOR_CMD();
    kprintf("%s @ galio: %s -# ", host, dir);
    SHELL_COLOR_RESET();
    shell_cursor_draw();
}

// Returns the index in the circular history buffer for a given logical index (0 = oldest, count-1 = newest)
static u32 shell_history_array_index(u32 logical_index) {
    if (logical_index >= history.count) return 0;
    u32 start = (history.head + HISTORY_SIZE - history.count) % HISTORY_SIZE;
    return (start + logical_index) % HISTORY_SIZE;
}

static void shell_add_history(const char *cmd) {
    if (input.len == 0) return;
    u32 idx = history.head;
    strncpy(history.history[idx], cmd, HISTORY_BUFFER_SIZE - 1);
    history.history[idx][HISTORY_BUFFER_SIZE - 1] = 0;
    history.head = (history.head + 1) % HISTORY_SIZE;
    if (history.count < HISTORY_SIZE) history.count++;
    history.index = history.count;
}

static void shell_clear_line(void) {
    shell_cursor_restore();
    for (u32 i = 0; i < input.len; i++) {
        vga_putch('\b');
        vga_putch(' ');
        vga_putch('\b');
    }
    shell_cursor_draw();
}

static void shell_print_buffer(void);

static void shell_print_history_entry(void) {
    if (history.index >= history.count) {
        shell_clear_line();
        input.len = 0;
        input.buffer[0] = 0;
        return;
    }

    shell_clear_line();
    u32 idx = shell_history_array_index(history.index);
    strncpy(input.buffer, history.history[idx], SHELL_BUFFER_SIZE - 1);
    input.buffer[SHELL_BUFFER_SIZE - 1] = 0;
    input.len = strlen(input.buffer);
    if (input.len >= SHELL_BUFFER_SIZE) input.len = SHELL_BUFFER_SIZE - 1;
    shell_print_buffer();
}

static void shell_history_prev(void) {
    if (history.index > 0) {
        history.index--;
        shell_print_history_entry();
    }
}

static void shell_history_next(void) {
    if (history.index < history.count - 1) {
        history.index++;
        shell_print_history_entry();
    } else if (history.index == history.count - 1) {
        history.index = history.count;
        shell_clear_line();
        input.len = 0;
        input.buffer[0] = 0;
    }
}

static void shell_print_buffer(void) {
    shell_cursor_restore();
    for (u32 i = 0; i < input.len; i++) {
        vga_putch(input.buffer[i]);
    }
    shell_cursor_draw();
}

static void shell_print_cursor(void) {
    shell_cursor_restore();
    shell_cursor_draw();
}

static void shell_poll_mouse(void) {
    mouse_poll_position();

    s8 scroll = mouse_get_scroll_delta();
    if (scroll > 0) {
        vga_scrollback_down();
    } else if (scroll < 0) {
        vga_scrollback_up();
    }
}

static const char *shell_basename(const char *path) {
    const char *base = path;
    while (*path) {
        if (*path == '/') base = path + 1;
        path++;
    }
    return base;
}

static const char *shell_display_dir(const char *path, char *out, u32 out_size) {
    if (!path || !out || out_size == 0) return ".";
    if (strcmp(path, ROOT_DIR) == 0) {
        out[0] = '.';
        out[1] = 0;
        return out;
    }
    if (strcmp(path, HOME_DIR) == 0) {
        strncpy(out, "/home", out_size - 1);
        out[out_size - 1] = 0;
        return out;
    }
    u32 home_len = strlen(HOME_DIR);
    if (strlen(path) > home_len && strncmp(path, HOME_DIR, home_len) == 0 && path[home_len] == '/') {
        if (out_size > 1) {
            strncpy(out, "/home", out_size - 1);
            out[out_size - 1] = 0;
            strncat(out, path + home_len, out_size - strlen(out) - 1);
        }
        return out;
    }
    if (shell_is_direct_root_child(path)) {
        strncpy(out, path, out_size - 1);
        out[out_size - 1] = 0;
        return out;
    }
    if (strncmp(path, "./", 2) == 0) {
        strncpy(out, path + 2, out_size - 1);
        out[out_size - 1] = 0;
        return out;
    }
    strncpy(out, path, out_size - 1);
    out[out_size - 1] = 0;
    return out;
}

static void shell_resolve_path(const char *cwd, const char *path, char *out) {
    if (!out) return;
    if (path_resolve(cwd, path, out, DIR_PATH_SIZE) == NULL) {
        out[0] = 0;
    }
}

static void shell_parent_dir(const char *path, char *out_parent) {
    if (!path || !out_parent) return;
    path_parent(path, out_parent, DIR_PATH_SIZE);
}

static u8 shell_is_direct_root_child(const char *path) {
    if (!path || *path == 0) return 0;
    char normalized[DIR_PATH_SIZE];
    if (!path_normalize(path, normalized, sizeof(normalized))) return 0;
    if (strcmp(normalized, ".") == 0) return 0;

    char parent[DIR_PATH_SIZE];
    path_parent(normalized, parent, sizeof(parent));
    return strcmp(parent, ".") == 0;
}

static u8 shell_find_ancestor_dir(const char *path, const char *target, char *out_match) {
    if (!path || !target || !*target) return 0;
    if (strcmp(target, ".") == 0) return 0;

    char candidate[DIR_PATH_SIZE];
    strncpy(candidate, path, DIR_PATH_SIZE - 1);
    candidate[DIR_PATH_SIZE - 1] = 0;

    while (strcmp(candidate, ".") != 0) {
        char base[DIR_PATH_SIZE];
        const char *bn = shell_basename(candidate);
        strncpy(base, bn, DIR_PATH_SIZE - 1);
        base[DIR_PATH_SIZE - 1] = 0;
        if (strcmp(base, target) == 0) {
            strncpy(out_match, candidate, DIR_PATH_SIZE - 1);
            out_match[DIR_PATH_SIZE - 1] = 0;
            return 1;
        }
        shell_parent_dir(candidate, candidate);
    }

    return 0;
}

static u8 shell_ensure_directories(const char *path) {
    if (!path) return 1;

    char normalized[DIR_PATH_SIZE];
    if (!path_normalize(path, normalized, sizeof(normalized))) return 0;
    if (strcmp(normalized, ".") == 0) return 1;

    char current[DIR_PATH_SIZE] = ".";
    const char *cursor = normalized;
    if (normalized[0] == '.' && normalized[1] == '/') cursor += 2;
    else if (normalized[0] == '/') cursor += 1;

    while (*cursor) {
        char segment[DIR_PATH_SIZE];
        u32 len = 0;
        while (cursor[len] && cursor[len] != '/' && len + 1 < sizeof(segment)) {
            segment[len] = cursor[len];
            len++;
        }
        segment[len] = 0;

        char next[DIR_PATH_SIZE];
        next[0] = 0;
        if (strcmp(current, ".") == 0) {
            strncpy(next, ".", sizeof(next) - 1);
            next[sizeof(next) - 1] = 0;
            if (*segment) {
                strncat(next, "/", sizeof(next) - strlen(next) - 1);
                strncat(next, segment, sizeof(next) - strlen(next) - 1);
            }
        } else {
            strncpy(next, current, sizeof(next) - 1);
            next[sizeof(next) - 1] = 0;
            strncat(next, "/", sizeof(next) - strlen(next) - 1);
            strncat(next, segment, sizeof(next) - strlen(next) - 1);
        }

        if (!vfs_is_dir(next)) {
            vfs_entry_t *entry = vfs_find(next);
            if (entry) return 0;
            if (!vfs_mkdir(next, 1)) return 0;
        }

        strncpy(current, next, DIR_PATH_SIZE - 1);
        current[DIR_PATH_SIZE - 1] = 0;

        if (cursor[len] == '/') cursor += len + 1;
        else break;
    }

    return 1;
}

static u8 is_root_child_path(const char *path) {
    if (!path || *path == 0) return 0;
    char normalized[DIR_PATH_SIZE];
    if (!path_normalize(path, normalized, sizeof(normalized))) return 0;
    if (strcmp(normalized, ".") == 0) return 0;

    char parent[DIR_PATH_SIZE];
    path_parent(normalized, parent, sizeof(parent));
    return strcmp(parent, ".") == 0;
}

/* --- Pipeline & background job support --- */
#define MAX_PIPE_STAGES 8
#define MAX_BG_JOBS 16

typedef struct {
    int id;
    int pids[MAX_PIPE_STAGES];
    int nprocs;
    char cmdline[256];
    int running;
} shell_job_t;

static shell_job_t bg_jobs[MAX_BG_JOBS];
static int next_job_id = 1;

/* Syscall helpers (inline int $0x80 wrappers) */
#define SYS_FORK 4
#define SYS_WAITPID 7
#define SYS_EXECVE 16
#define SYS_PIPE 17
#define SYS_DUP2 19
#define SYS_CLOSE 10
#define SYS_EXIT 1

static inline int sc_syscall1(int num, int a1) {
    int ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(num), "b"(a1)
        : "memory"
    );
    return ret;
}

static inline int sc_syscall2(int num, int a1, int a2) {
    int ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(num), "b"(a1), "c"(a2)
        : "memory"
    );
    return ret;
}

static inline int sc_syscall3(int num, int a1, int a2, int a3) {
    int ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(num), "b"(a1), "c"(a2), "d"(a3)
        : "memory"
    );
    return ret;
}

static int shell_parse_pipeline(const char *line, char out_cmds[][256], int *out_n, int *out_bg) {
    int n = 0;
    int bg = 0;
    int in_sq = 0, in_dq = 0;
    int len = strlen(line);
    char cur[256];
    int ci = 0;

    /* Detect trailing & (after trimming whitespace) */
    int last = len - 1;
    while (last >= 0 && (line[last] == ' ' || line[last] == '\t')) last--;
    if (last >= 0 && line[last] == '&') {
        bg = 1;
        /* ignore the & when parsing; treat as removed */
        len = last; /* exclusive end */
    }

    for (int i = 0; i < len; i++) {
        char c = line[i];
        if (c == '\'' && !in_dq) { in_sq = !in_sq; cur[ci++] = c; continue; }
        if (c == '"' && !in_sq) { in_dq = !in_dq; cur[ci++] = c; continue; }
        if (c == '|' && !in_sq && !in_dq) {
            /* end current */
            while (ci > 0 && (cur[ci-1] == ' ' || cur[ci-1] == '\t')) ci--;
            cur[ci] = 0;
            /* trim leading */
            int s = 0; while (cur[s] == ' ' || cur[s] == '\t') s++;
            strncpy(out_cmds[n], cur + s, 255);
            out_cmds[n][255] = 0;
            n++;
            if (n >= MAX_PIPE_STAGES) break;
            ci = 0;
            continue;
        }
        if (ci < (int)sizeof(cur) - 1) cur[ci++] = c;
    }

    if (ci > 0) {
        while (ci > 0 && (cur[ci-1] == ' ' || cur[ci-1] == '\t')) ci--;
        cur[ci] = 0;
        int s = 0; while (cur[s] == ' ' || cur[s] == '\t') s++;
        strncpy(out_cmds[n], cur + s, 255);
        out_cmds[n][255] = 0;
        n++;
    }

    *out_n = n;
    *out_bg = bg;
    return 0;
}

static void shell_job_add(int *pids, int n, const char *cmdline) {
    for (int i = 0; i < MAX_BG_JOBS; i++) {
        if (!bg_jobs[i].running) {
            bg_jobs[i].running = 1;
            bg_jobs[i].id = next_job_id++;
            bg_jobs[i].nprocs = n < MAX_PIPE_STAGES ? n : MAX_PIPE_STAGES;
            for (int j = 0; j < bg_jobs[i].nprocs; j++) bg_jobs[i].pids[j] = pids[j];
            strncpy(bg_jobs[i].cmdline, cmdline, sizeof(bg_jobs[i].cmdline)-1);
            bg_jobs[i].cmdline[sizeof(bg_jobs[i].cmdline)-1] = 0;
            kprintf("[%d]", bg_jobs[i].id);
            for (int j = 0; j < bg_jobs[i].nprocs; j++) kprintf(" %d", bg_jobs[i].pids[j]);
            kprintf("\n");
            return;
        }
    }
    SHELL_COLOR_ERR(); kprintf("No space for more background jobs\n"); SHELL_COLOR_RESET();
}

static void shell_list_jobs(void) {
    for (int i = 0; i < MAX_BG_JOBS; i++) {
        if (bg_jobs[i].running) {
            kprintf("[%d]", bg_jobs[i].id);
            for (int j = 0; j < bg_jobs[i].nprocs; j++) kprintf(" %d", bg_jobs[i].pids[j]);
            kprintf("  %s\n", bg_jobs[i].cmdline);
        }
    }
}

static shell_job_t *shell_find_job_by_id(int id) {
    for (int i = 0; i < MAX_BG_JOBS; i++) {
        if (bg_jobs[i].running && bg_jobs[i].id == id) return &bg_jobs[i];
    }
    return NULL;
}

static void shell_remove_job(shell_job_t *job) {
    if (!job) return;
    job->running = 0;
    job->nprocs = 0;
    job->cmdline[0] = 0;
}

/* Execute pipeline of commands (each entry is a full command string). */
static void shell_run_pipeline(char cmds[][256], int n, int bg) {
    if (n <= 0) return;
    if (n > MAX_PIPE_STAGES) {
        SHELL_COLOR_ERR(); kprintf("Too many pipeline stages (max %d)\n", MAX_PIPE_STAGES); SHELL_COLOR_RESET();
        return;
    }

    int pipefds[MAX_PIPE_STAGES-1][2];
    for (int i = 0; i < n-1; i++) {
        int r = sc_syscall1(SYS_PIPE, (int)&pipefds[i]);
        if (r < 0) {
            SHELL_COLOR_ERR(); kprintf("pipe() failed\n"); SHELL_COLOR_RESET();
            return;
        }
    }

    int child_pids[MAX_PIPE_STAGES];

    for (int i = 0; i < n; i++) {
        int pid = sc_syscall1(SYS_FORK, 0);
        if (pid == 0) {
            /* Child */
            /* stdin from previous pipe */
            if (i > 0) {
                sc_syscall2(SYS_DUP2, pipefds[i-1][0], 0);
            }
            /* stdout to next pipe */
            if (i < n-1) {
                sc_syscall2(SYS_DUP2, pipefds[i][1], 1);
            }
            /* Close all pipe fds */
            for (int j = 0; j < n-1; j++) {
                sc_syscall1(SYS_CLOSE, pipefds[j][0]);
                sc_syscall1(SYS_CLOSE, pipefds[j][1]);
            }

            /* Build argv (simple split, respecting quotes) */
            char *argv[16];
            char argbuf[256];
            char argv_storage[16][256];
            int argc = 0;
            int ai = 0;
            int in_sq = 0, in_dq = 0;
            char *s = cmds[i];
            for (int k = 0; ; k++) {
                char c = s[k];
                if (c == '\0' || (c == ' ' && !in_sq && !in_dq)) {
                    if (ai > 0) {
                        argbuf[ai] = 0;
                        if (ai >= (int)sizeof(argv_storage[argc])) ai = sizeof(argv_storage[argc]) - 1;
                        strncpy(argv_storage[argc], argbuf, ai + 1);
                        argv_storage[argc][ai] = 0;
                        argv[argc] = argv_storage[argc];
                        argc++;
                        ai = 0;
                    }
                    if (c == '\0') break;
                    continue;
                }
                if (c == '\'' && !in_dq) { in_sq = !in_sq; continue; }
                if (c == '"' && !in_sq) { in_dq = !in_dq; continue; }
                if (ai < (int)sizeof(argbuf)-1) argbuf[ai++] = c;
            }
            argv[argc] = NULL;

            if (argc == 0) sc_syscall1(SYS_EXIT, 1);

            /* Resolve binary path: if contains '/', use as-is; else prefix /bin/ */
            char pathbuf[256];
            if (shell_strchr(argv[0], '/')) {
                strncpy(pathbuf, argv[0], sizeof(pathbuf)-1);
                pathbuf[sizeof(pathbuf)-1] = 0;
            } else {
                strncpy(pathbuf, "/bin/", sizeof(pathbuf)-1);
                pathbuf[sizeof(pathbuf)-1] = 0;
                strncat(pathbuf, argv[0], sizeof(pathbuf) - strlen(pathbuf) - 1);
            }

            sc_syscall3(SYS_EXECVE, (int)pathbuf, (int)argv, 0);
            SHELL_COLOR_ERR(); kprintf("Failed to exec: %s\n", pathbuf); SHELL_COLOR_RESET();
            sc_syscall1(SYS_EXIT, 1);
        } else if (pid > 0) {
            child_pids[i] = pid;
        } else {
            SHELL_COLOR_ERR(); kprintf("fork() failed\n"); SHELL_COLOR_RESET();
            /* close pipes */
            for (int j = 0; j < n-1; j++) {
                sc_syscall1(SYS_CLOSE, pipefds[j][0]);
                sc_syscall1(SYS_CLOSE, pipefds[j][1]);
            }
            return;
        }
    }

    /* Parent closes all pipe fds */
    for (int j = 0; j < n-1; j++) {
        sc_syscall1(SYS_CLOSE, pipefds[j][0]);
        sc_syscall1(SYS_CLOSE, pipefds[j][1]);
    }

    if (bg) {
        /* store job and return */
        /* join child pids into single command line string */
        char combined[256] = {0};
        for (int i = 0; i < n; i++) {
            if (i) strncat(combined, " | ", sizeof(combined)-strlen(combined)-1);
            strncat(combined, cmds[i], sizeof(combined)-strlen(combined)-1);
        }
        shell_job_add(child_pids, n, combined);
        return;
    }

    /* Foreground: wait for last child */
    int lastpid = child_pids[n-1];
    if (lastpid > 0) sc_syscall1(SYS_WAITPID, lastpid);
}

u8 shell_dir_command(const char *args, const char *current_dir, u8 replace, u8 privileged) {
    if (!args || *args == 0) {
        SHELL_COLOR_CMD();
        // Optional: show usage if desired, but keeping original style
        SHELL_COLOR_RESET();
        return 0;
    }

    char local[512];
    strncpy(local, args, sizeof(local) - 1);
    local[sizeof(local) - 1] = 0;

    char *ptr = local;
    u8 any_success = 0;

    while (*ptr) {
        while (*ptr == ' ') ptr++;
        if (*ptr == 0) break;

        char *end = ptr;
        while (*end && *end != ' ') end++;

        char saved_char = *end;
        *end = 0;

        char fullpath[DIR_PATH_SIZE];
        shell_resolve_path(current_dir, ptr, fullpath);

        /* Ensure parent directories exist */
        char parent[DIR_PATH_SIZE];
        path_parent(fullpath, parent, sizeof(parent));

        if (!privileged && is_root_child_path(fullpath)) {
            SHELL_COLOR_ERR();
            kprintf("[DIR] Permission denied: use 'rex dir %s' to create root-level directories\n", fullpath);
            SHELL_COLOR_RESET();
            *end = saved_char;
            ptr = end + 1;
            continue;
        }

        if (!vfs_is_dir(parent)) {
            if (!shell_ensure_directories(parent)) {
                SHELL_COLOR_ERR();
                kprintf("[DIR] Cannot create parent directories: %s\n", parent);
                SHELL_COLOR_RESET();
                *end = saved_char;
                ptr = end + 1;
                continue;
            }
        }

        vfs_entry_t *existing = vfs_find(fullpath);
        if (existing) {
            if (!existing->is_dir) {
                SHELL_COLOR_ERR();
                kprintf("[DIR] Path exists and is not a directory: %s\n", fullpath);
                SHELL_COLOR_RESET();
            } else {
                /* Directory already exists – treat as success (no error) */
                any_success = 1;
            }
        } else {
            if (vfs_mkdir(fullpath, replace)) {
                any_success = 1;
            }
        }

        *end = saved_char;
        ptr = end + 1;
    }

    return any_success;
}
/* Parse and execute command */
/* Parse and execute command */
static void shell_execute_command(void) {
    if (input.len == 0) return;

    if (input.len >= SHELL_BUFFER_SIZE) {
        input.len = SHELL_BUFFER_SIZE - 1;
    }
    input.buffer[input.len] = 0;
    shell_add_history(input.buffer);
    kprintf("\n");

    /* Parse for pipes and background '&' */
    {
        char cmds[MAX_PIPE_STAGES][256];
        int stages = 0;
        int bg = 0;
        shell_parse_pipeline(input.buffer, cmds, &stages, &bg);
        if (stages > 1) {
            /* disallow rex inside pipeline for now */
            for (int i = 0; i < stages; i++) {
                if (strncmp(cmds[i], "rex", 3) == 0 && (cmds[i][3] == ' ' || cmds[i][3] == '\0')) {
                    SHELL_COLOR_ERR(); kprintf("rex inside pipelines is not supported\n"); SHELL_COLOR_RESET();
                    shell_print_prompt();
                    input.len = 0;
                    return;
                }
            }
            shell_run_pipeline(cmds, stages, bg);
            shell_print_prompt();
            input.len = 0;
            return;
        } else if (stages == 1 && bg) {
            /* background single command: run as external */
            shell_run_pipeline(cmds, stages, bg);
            shell_print_prompt();
            input.len = 0;
            return;
        }
    }

    /* Handle rex (sudo-like) commands */
    if (strncmp(input.buffer, "rex", 3) == 0 && (input.buffer[3] == ' ' || input.buffer[3] == '\0')) {
        /* Check if already authorized in this session */
        if (!auth_is_authorized()) {
            /* Not authorized - prompt for password */
            SHELL_COLOR_CMD();
            kprintf("[REX] Privileged command requires authentication\n");
            SHELL_COLOR_RESET();
            
            char password[INPUT_BUFFER_SIZE];
            if (!auth_prompt_password("Password: ", password, INPUT_BUFFER_SIZE)) {
                SHELL_COLOR_ERR();
                kprintf("[REX] Authentication cancelled\n");
                SHELL_COLOR_RESET();
                shell_print_prompt();
                input.len = 0;
                return;
            }
            
            /* Verify password against kernel_auth credentials */
            if (!auth_verify_password(kernel_auth.username, password)) {
                SHELL_COLOR_ERR();
                kprintf("[REX] Access denied: Invalid password\n");
                SHELL_COLOR_RESET();
                shell_print_prompt();
                input.len = 0;
                return;
            }
            
            /* Password correct - authorize for this session */
            auth_authorize();
            SHELL_COLOR_CMD();
            kprintf("[REX] Password accepted. Privileged mode enabled for this session.\n");
            SHELL_COLOR_RESET();
        }
        
        /* Now execute the privileged command (already authorized) */
        const char *cmd = input.buffer + 3;
        
        /* Skip leading spaces */
        while (*cmd == ' ') cmd++;
        
        if (strlen(cmd) == 0) {
            SHELL_COLOR_ERR();
            kprintf("[REX] Usage: rex <command> [args...]\n");
            kprintf("[REX] Available commands: goto, file, new file, dir, new dir, mkdir, write, recycle, delete, clean\n");
            SHELL_COLOR_RESET();
        } else if (strncmp(cmd, "goto ", 5) == 0) {
            const char *path = cmd + 5;
            while (*path == ' ') path++;
            shell_resolve_path(current_dir, path, current_dir);
            current_dir[255] = 0;
            SHELL_COLOR_CMD();
            kprintf("[REX] Changed to: %s\n", current_dir);
            SHELL_COLOR_RESET();
        } else if (strncmp(cmd, "file", 4) == 0 && (cmd[4] == ' ' || cmd[4] == '\0')) {
            const char *file_args = cmd + 4;
            if (*file_args == ' ') file_args++;
            SHELL_COLOR_OUT();
            shell_file_command(file_args, current_dir, 1, 1);
            SHELL_COLOR_RESET();
        } else if (strncmp(cmd, "new file", 8) == 0) {
            const char *file_args = cmd + 8;
            if (*file_args == ' ') file_args++;
            SHELL_COLOR_OUT();
            shell_file_command(file_args, current_dir, 1, 1);
            SHELL_COLOR_RESET();
        } else if (strncmp(cmd, "dir", 3) == 0 && (cmd[3] == ' ' || cmd[3] == '\0')) {
            const char *dir_args = cmd + 3;
            if (*dir_args == ' ') dir_args++;
            shell_dir_command(dir_args, current_dir, 1, 1);
        } else if (strncmp(cmd, "new dir", 7) == 0) {
            const char *dir_args = cmd + 7;
            if (*dir_args == ' ') dir_args++;
            shell_dir_command(dir_args, current_dir, 1, 1);
        } else if (strncmp(cmd, "mkdir", 5) == 0 && (cmd[5] == ' ' || cmd[5] == '\0')) {
            const char *dir_args = cmd + 5;
            if (*dir_args == ' ') dir_args++;
            shell_dir_command(dir_args, current_dir, 1, 1);
        } else if (strncmp(cmd, "new mkdir", 9) == 0) {
            const char *dir_args = cmd + 9;
            if (*dir_args == ' ') dir_args++;
            shell_dir_command(dir_args, current_dir, 1, 1);
        } else if (strncmp(cmd, "write", 5) == 0 && (cmd[5] == ' ' || cmd[5] == '\0')) {
            const char *write_args = cmd + 5;
            if (*write_args == ' ') write_args++;
            SHELL_COLOR_OUT();
            shell_write_command(write_args, current_dir, 1);
            SHELL_COLOR_RESET();
        } else if (strncmp(cmd, "recycle", 7) == 0 && (cmd[7] == ' ' || cmd[7] == '\0')) {
            const char *recycle_args = cmd + 7;
            if (*recycle_args == ' ') recycle_args++;
            SHELL_COLOR_OUT();
            shell_recycle_command(recycle_args, current_dir, 1);
            SHELL_COLOR_RESET();
        } else if (strncmp(cmd, "delete", 6) == 0 && (cmd[6] == ' ' || cmd[6] == '\0')) {
            const char *delete_args = cmd + 6;
            if (*delete_args == ' ') delete_args++;
            SHELL_COLOR_OUT();
            shell_delete_command(delete_args, current_dir, 1);
            SHELL_COLOR_RESET();
        } else if (strncmp(cmd, "clean", 5) == 0 && (cmd[5] == ' ' || cmd[5] == '\0')) {
            const char *clean_args = cmd + 5;
            if (*clean_args == ' ') clean_args++;
            SHELL_COLOR_OUT();
            shell_clean_command(clean_args, current_dir);
            SHELL_COLOR_RESET();
        } else {
            SHELL_COLOR_ERR();
            kprintf("[REX] Unknown privileged command: %s\n", cmd);
            kprintf("[REX] Available: goto, file, new file, dir, new dir, mkdir, write, recycle, delete, clean\n");
            SHELL_COLOR_RESET();
        }
    } else if (strcmp(input.buffer, "jobs") == 0) {
        shell_list_jobs();
    } else if (strncmp(input.buffer, "fg ", 3) == 0) {
        const char *arg = input.buffer + 3;
        while (*arg == ' ') arg++;
        int id = shell_atoi(arg);
        if (id <= 0) {
            SHELL_COLOR_ERR(); kprintf("Usage: fg <job_id>\n"); SHELL_COLOR_RESET();
        } else {
            shell_job_t *job = shell_find_job_by_id(id);
            if (!job) {
                SHELL_COLOR_ERR(); kprintf("No such job: %d\n", id); SHELL_COLOR_RESET();
            } else {
                int last = job->pids[job->nprocs - 1];
                if (last > 0) sc_syscall1(SYS_WAITPID, last);
                shell_remove_job(job);
            }
        }
    } else if (strncmp(input.buffer, "new ", 4) == 0) {
        SHELL_COLOR_OUT();
        shell_new_command(input.buffer + 4, current_dir, 0);
        SHELL_COLOR_RESET();
    } else if (strcmp(input.buffer, "new") == 0) {
        SHELL_COLOR_OUT();
        shell_new_command("", current_dir, 0);
        SHELL_COLOR_RESET();
    } else if (strncmp(input.buffer, "file ", 5) == 0) {
        SHELL_COLOR_OUT();
        shell_file_command(input.buffer + 5, current_dir, 0, 0);
        SHELL_COLOR_RESET();
    } else if (strcmp(input.buffer, "file") == 0) {
        SHELL_COLOR_OUT();
        shell_file_command("", current_dir, 0, 0);
        SHELL_COLOR_RESET();
    } else if (strncmp(input.buffer, "write ", 6) == 0) {
        SHELL_COLOR_OUT();
        shell_write_command(input.buffer + 6, current_dir, 0);
        SHELL_COLOR_RESET();
    } else if (strcmp(input.buffer, "write") == 0) {
        SHELL_COLOR_OUT();
        shell_write_command("", current_dir, 0);
        SHELL_COLOR_RESET();
    } else if (strncmp(input.buffer, "show ", 5) == 0) {
        SHELL_COLOR_OUT();
        shell_show_command(input.buffer + 5, current_dir);
        SHELL_COLOR_RESET();
    } else if (strcmp(input.buffer, "show") == 0) {
        SHELL_COLOR_CMD();
        kprintf("[SHOW] Usage: show <filepath>\n");
        kprintf("[SHOW] Example: show ./usr/home/Desktop/file.txt\n");
        SHELL_COLOR_RESET();
    } else if (strncmp(input.buffer, "recycle ", 8) == 0) {
        SHELL_COLOR_OUT();
        shell_recycle_command(input.buffer + 8, current_dir, 0);
        SHELL_COLOR_RESET();
    } else if (strcmp(input.buffer, "recycle") == 0) {
        SHELL_COLOR_OUT();
        shell_recycle_command("", current_dir, 0);
        SHELL_COLOR_RESET();
    } else if (strncmp(input.buffer, "delete ", 7) == 0) {
        SHELL_COLOR_OUT();
        shell_delete_command(input.buffer + 7, current_dir, 0);
        SHELL_COLOR_RESET();
    } else if (strcmp(input.buffer, "delete") == 0) {
        SHELL_COLOR_OUT();
        shell_delete_command("", current_dir, 0);
        SHELL_COLOR_RESET();
    } else if (strncmp(input.buffer, "clean ", 6) == 0) {
        SHELL_COLOR_OUT();
        shell_clean_command(input.buffer + 6, current_dir);
        SHELL_COLOR_RESET();
    } else if (strcmp(input.buffer, "clean") == 0) {
        SHELL_COLOR_OUT();
        shell_clean_command("", current_dir);
        SHELL_COLOR_RESET();
    } else if (strncmp(input.buffer, "net ", 4) == 0) {
        SHELL_COLOR_OUT();
        shell_net_command(input.buffer + 4, current_dir);
        SHELL_COLOR_RESET();
    } else if (strcmp(input.buffer, "net") == 0) {
        SHELL_COLOR_CMD();
        kprintf("Usage: net <stat|scan|list|devices>\n");
        SHELL_COLOR_RESET();
    } else if (strncmp(input.buffer, "pkg ", 4) == 0) {
        SHELL_COLOR_OUT();
        shell_pkg_command(input.buffer + 4, current_dir);
        SHELL_COLOR_RESET();
    } else if (strcmp(input.buffer, "pkg") == 0) {
        SHELL_COLOR_CMD();
        kprintf("Usage: pkg <list|install|remove> [package_name]\n");
        SHELL_COLOR_RESET();
    } else if (strncmp(input.buffer, "clear", 5) == 0) {
        shell_cursor_restore();
        vga_clear();
        SHELL_COLOR_OUT();
    } else if (strcmp(input.buffer, "tree") == 0) {
        SHELL_COLOR_OUT();
        shell_tree_command(current_dir);
        SHELL_COLOR_RESET();
        kprintf("                                                                     \n");
        kprintf("                                                                     \n");
        SHELL_COLOR_RESET();
    } else if (strncmp(input.buffer, "help", 4) == 0) {
        SHELL_COLOR_OUT();
        kprintf("\n__________________________________________________________\n");
        kprintf(" |                  Available Commands:                   |\n");
        kprintf(" |________________________________________________________|\n");
        kprintf(" |  ls       - List directory contents                    |\n");
        kprintf(" |________________________________________________________|\n");
        kprintf(" |  tree     - Show directory tree for current directory   |\n");
        kprintf(" |_________________________________________________________|\n");
        kprintf(" |  dir     - Create directory (usage: dir <path1> [path2])|\n");
        kprintf(" |________________________________________________________ |n");
        kprintf(" |  rmdir    - Remove directory (usage: rmdir <path>)      |\n");
        kprintf(" |_________________________________________________________|\n");
        kprintf(" |  file     - Create file (file <name>[.ext] [name...])   |\n");
        kprintf(" |_________________________________________________________|\n");
        kprintf(" |  new file - Create or replace file (usage: new file <name>...)|\n");
        kprintf(" |_____________________________________________________________ _|\n");
        kprintf(" |  new dir  - Create directory (usage: new dir <name> [name...])|\n");
        kprintf(" |_______________________________________________________________|\n");
        kprintf(" |  write    - Write/edit file (usage: write <name> [path]) |\n");
        kprintf(" |__________________________________________________________|\n");
        kprintf(" |  show     - Display file contents (usage: show <filepath>)       |\n");
        kprintf(" |__________________________________________________________________|\n");
        kprintf(" | recycle  - Move to recycle bin (usage: recycle <path1> [path2])  |\n");
        kprintf(" |__________________________________________________________________|\n");
        kprintf(" | clean    - Clean recycle bin (usage: clean rbin)                 |\n");
        kprintf(" |__________________________________________________________________|\n");
        kprintf(" |  net      - Networking commands (usage: net stat|scan|list|devices)     |\n");
        kprintf(" |________________________________________________________|\n");
        kprintf(" | delete   - Permanently delete (usage: delete <path1> [path2])|\n");
        kprintf(" |________________________________________________________|\n");
        kprintf(" | clear    - Clear the screen                            |\n");
        kprintf(" |________________________________________________________|\n");
        kprintf(" | echo     - Echo text (usage: echo <text>)              |\n");
        kprintf(" |________________________________________________________|\n");
        kprintf(" | uname    - Show system name                            |\n");
        kprintf(" |___________________________ ____________________________|\n");
        kprintf(" | pwd      - Print current directory                     |\n");
        kprintf(" |________________________________________________________|\n");
        kprintf(" | goto     - Change directory (usage: goto <path>)       |\n");
        kprintf(" |________________________________________________________|\n");
        kprintf(" | back  - Go back to previous dir (usage: back [dirname])|\n");
        kprintf(" |_______________________________________________________ |\n");
        kprintf(" | rex      - Privileged command (like sudo)              |\n");
        kprintf(" |           Usage: rex <cmd> [args]                      |\n");
        kprintf(" |           Password required once per session           |\n");
        kprintf(" |________________________________________________________|\n");
        kprintf(" |  Use UP/DOWN arrows to navigate history                |\n");
        kprintf(" |________________________________________________________|\n");
        kprintf("\n");
        SHELL_COLOR_RESET();
    } else if (strcmp(input.buffer, "exit") == 0) {
        shell_cursor_restore();
        shell_should_exit = 1;
        return;
    } else if (strncmp(input.buffer, "ls", 2) == 0) {
        SHELL_COLOR_OUT();
        vfs_listdir(current_dir);
        SHELL_COLOR_RESET();
    } else if (strncmp(input.buffer, "dir ", 4) == 0) {
        shell_dir_command(input.buffer + 4, current_dir, 0, 0);
    } else if (strcmp(input.buffer, "dir") == 0) {
        shell_dir_command("", current_dir, 0, 0);
    } else if (strncmp(input.buffer, "mkdir ", 6) == 0) {
        shell_dir_command(input.buffer + 6, current_dir, 0, 0);
    } else if (strcmp(input.buffer, "mkdir") == 0) {
        shell_dir_command("", current_dir, 0, 0);
    } else if (strncmp(input.buffer, "rmdir ", 6) == 0) {
        const char *dirname = input.buffer + 6;
        char fullpath[DIR_PATH_SIZE];
        shell_resolve_path(current_dir, dirname, fullpath);
        SHELL_COLOR_OUT();
        vfs_rmdir(fullpath);
        SHELL_COLOR_RESET();
    } else if (strncmp(input.buffer, "pwd", 3) == 0) {
        SHELL_COLOR_OUT();
        kprintf("%s\n", current_dir);
        SHELL_COLOR_RESET();
    } else if (strncmp(input.buffer, "goto ", 5) == 0) {
        const char *dirname = input.buffer + 5;
        char fullpath[DIR_PATH_SIZE];
        shell_resolve_path(current_dir, dirname, fullpath);

        if (strcmp(fullpath, ROOT_DIR) == 0) {
            SHELL_COLOR_ERR();
            kprintf("Permission denied: use 'rex goto .' to access root\n");
            SHELL_COLOR_RESET();
        } else if (vfs_is_dir(fullpath)) {
            if (dir_history.sp < DIR_HISTORY_SIZE) {
                strncpy(dir_history.stack[dir_history.sp], current_dir, DIR_PATH_SIZE - 1);
                dir_history.stack[dir_history.sp][DIR_PATH_SIZE - 1] = 0;
                dir_history.sp++;
            }
            strncpy(current_dir, fullpath, 255);
            current_dir[255] = 0;
        } else {
            SHELL_COLOR_ERR();
            kprintf("Directory not found: %s\n", fullpath);
            SHELL_COLOR_RESET();
        }
    } else if (strncmp(input.buffer, "back", 4) == 0) {
        const char *target = input.buffer + 4;
        while (*target == ' ') target++;

        if (*target == 0) {
            if (dir_history.sp > 0) {
                dir_history.sp--;
                strncpy(current_dir, dir_history.stack[dir_history.sp], 255);
                current_dir[255] = 0;
            } else {
                char parent[DIR_PATH_SIZE];
                path_parent(current_dir, parent, DIR_PATH_SIZE);
                if (strcmp(parent, current_dir) != 0 && strcmp(current_dir, ROOT_DIR) != 0) {
                    if (strcmp(parent, ROOT_DIR) == 0) {
                        SHELL_COLOR_ERR();
                        kprintf("Permission denied: use 'rex goto .' to access root\n");
                        SHELL_COLOR_RESET();
                    } else {
                        strncpy(current_dir, parent, 255);
                        current_dir[255] = 0;
                    }
                } else {
                    SHELL_COLOR_ERR();
                    kprintf("No previous directory\n");
                    SHELL_COLOR_RESET();
                }
            }
        } else {
            if (strcmp(target, ".") == 0 || strcmp(target, "./") == 0) {
                SHELL_COLOR_ERR();
                kprintf("Permission denied: use 'rex goto .' to access root\n");
                SHELL_COLOR_RESET();
            } else {
                u32 found = 0;
                for (u32 i = dir_history.sp; i > 0; i--) {
                    u32 idx = i - 1;
                    if (strcmp(dir_history.stack[idx], target) == 0 ||
                        strcmp(shell_basename(dir_history.stack[idx]), target) == 0) {
                        dir_history.sp = idx;
                        strncpy(current_dir, dir_history.stack[idx], 255);
                        current_dir[255] = 0;
                        found = 1;
                        break;
                    }
                }

                if (!found) {
                    char match[DIR_PATH_SIZE];
                    if (shell_find_ancestor_dir(current_dir, target, match)) {
                        strncpy(current_dir, match, 255);
                        current_dir[255] = 0;
                        found = 1;
                    }
                }

                if (!found) {
                    SHELL_COLOR_ERR();
                    kprintf("Directory not in history: %s\n", target);
                    SHELL_COLOR_RESET();
                }
            }
        }
    } else if (strncmp(input.buffer, "echo ", 5) == 0) {
        SHELL_COLOR_OUT();
        kprintf("%s\n", input.buffer + 5);
        SHELL_COLOR_RESET();
    } else if (strncmp(input.buffer, "uname", 5) == 0) {
        SHELL_COLOR_OUT();
        kprintf("Galio v1.0\n");
        SHELL_COLOR_RESET();
    } else if (input.len > 0) {
        SHELL_COLOR_ERR();
        kprintf("Unknown command: %s\nType 'help' for available commands\n", input.buffer);
        SHELL_COLOR_RESET();
    }

    shell_print_prompt();
    input.len = 0;
}

/* Poll keyboard for input (no IRQs) */
static void shell_poll_keyboard(void) {
    u8 scancode;
    u8 is_pressed;
    u8 extended;

    if (keyboard_read_event(&scancode, &is_pressed, &extended)) {
        if (!is_pressed) {
            return;
        }

        /* Snap back to live screen on any key except PgUp/PgDn scroll keys */
        if (!(scancode == 0x49 || scancode == 0x51)) {
            vga_show_live_screen();
        }

        if (extended || scancode == 0x48 || scancode == 0x50 || scancode == 0x49 || scancode == 0x51) {
            if (scancode == 0x48) {
                shell_history_prev();
                return;
            } else if (scancode == 0x50) {
                shell_history_next();
                return;
            } else if (scancode == 0x49) {
                vga_scrollback_up();
                return;
            } else if (scancode == 0x51) {
                vga_scrollback_down();
                return;
            }
            return;
        }

        u8 c = scancode_to_ascii(scancode);
        if (c == 0) {
            return;
        }

        if (c == '\b') {
            if (input.len > 0) {
                shell_cursor_restore();
                input.len--;
                input.buffer[input.len] = 0;
                vga_putch('\b');
                vga_putch(' ');
                vga_putch('\b');
                shell_cursor_draw();
            }
        } else if (c == '\n') {
            shell_cursor_restore();
            vga_putch('\n');
            shell_execute_command();
        } else if (c == '\t') {
            return;
        } else if (c >= 32 && c < 127) {
            if (input.len < SHELL_BUFFER_SIZE - 1) {
                shell_cursor_restore();
                input.buffer[input.len] = c;
                input.len++;
                input.buffer[input.len] = 0;
                vga_putch(c);
                shell_cursor_draw();
            }
        }
        return;
    }
}

void shell_run(void) {
    input.len = 0;
    input.buffer[0] = 0;
    shell_should_exit = 0;

    vga_clear();

    strncpy(current_dir, HOME_DIR, sizeof(current_dir) - 1);
    current_dir[sizeof(current_dir) - 1] = 0;

    vfs_cleanup_old_recycle_bin("./usr/home/desktop/recycle", 259200000);

    SHELL_COLOR_OUT();
    SHELL_COLOR_RESET();

    shell_print_prompt();
    shell_cursor_reset();

    while (!shell_should_exit) {
        shell_poll_mouse();
        shell_poll_keyboard();
        for (volatile int i = 0; i < 100; i++);
    }

    shell_cursor_restore();
    shell_cursor_drawn = 0;
    vga_disable_hardware_cursor();  /* Disable hardware cursor; UI manages its own */
    vga_clear();
}
