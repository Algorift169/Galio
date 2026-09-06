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
#include "ip.h"
#include "pkg.h"
#include "where.h"
#include "editor.h"
#include "process.h"
#include "syscall_cmd.h"
#include "wifi_list.h"
#include "top.h"
#include "spike.h"
#include "user_syscall.h"
#include "kernel_time.h"
#include "power/power.h"
#include "options.h"
#include "cpufreq_cmd.h"
#include "gc.h"
#include "script.h"

u8 shell_net_command(const char *args, const char *current_dir);
u8 shell_pkg_command(const char *args, const char *current_dir);
u8 shell_chuser_command(const char *args, const char *current_dir);
u8 shell_passwd_command(const char *args, const char *current_dir);

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
    u32 cursor;
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
static u8 shell_rex_command = 0;
static u8 shell_script_mode = 0;
static u8 shell_drift_mode = 0;
static int shell_last_status = 0;
static int shell_cursor_x = 0;
static int shell_cursor_y = 0;
static int shell_input_origin_x = 0;
static int shell_input_origin_y = 0;
static u32 shell_rendered_input_len = 0;
static unsigned short shell_cursor_saved_cell = 0;
static u8 shell_cursor_drawn = 0;

static const char *shell_display_dir(const char *path, char *out, u32 out_size);
static u8 shell_is_direct_root_child(const char *path);

static u8 shell_is_gh_script_command(const char *command, char *path, u32 path_size) {
    const char *start = command;
    const char *end;
    u32 length;
    if (!command || !path || path_size == 0) return 0;
    while (*start == ' ' || *start == '\t') start++;
    end = start;
    while (*end && *end != ' ' && *end != '\t') end++;
    length = (u32)(end - start);
        if (length < 3 || start[length - 3] != '.' ||
                !((start[length - 2] == 'g' && start[length - 1] == 'h') ||
                    (start[length - 2] == 'd' && start[length - 1] == 'f'))) return 0;
    if (length >= path_size) length = path_size - 1;
    memcpy(path, start, length);
    path[length] = 0;
    return 1;
}

static int shell_has_logical_operator(const char *line) {
    int in_single_quote = 0;
    int in_double_quote = 0;
    u32 i;
    for (i = 0; line[i]; i++) {
        if (line[i] == '\'' && !in_double_quote) in_single_quote = !in_single_quote;
        if (line[i] == '"' && !in_single_quote) in_double_quote = !in_double_quote;
        if (!in_single_quote && !in_double_quote &&
            (line[i] == ';' || (line[i] == '&' && line[i + 1] == '&') ||
             (line[i] == '|' && line[i + 1] == '|'))) return 1;
    }
    return 0;
}

static void shell_normalize_input(void) {
    u32 start = 0;
    while (start < input.len && (input.buffer[start] == ' ' || input.buffer[start] == '\t')) start++;
    while (input.len > start && (input.buffer[input.len - 1] == ' ' || input.buffer[input.len - 1] == '\t')) input.len--;
    if (start > 0) {
        memmove(input.buffer, input.buffer + start, input.len - start);
        input.len -= start;
    }
    input.buffer[input.len] = 0;
    if (input.cursor > input.len) input.cursor = input.len;
}

static u8 shell_is_mutating_command(const char *command)
{
    const char *end = command;
    char name[16];
    u32 length;

    while (*end && *end != ' ' && *end != '\t') end++;
    length = (u32)(end - command);
    if (length == 0 || length >= sizeof(name)) return 0;
    memcpy(name, command, length);
    name[length] = 0;

    return strcmp(name, "new") == 0 || strcmp(name, "file") == 0 ||
           strcmp(name, "write") == 0 || strcmp(name, "edit") == 0 ||
           strcmp(name, "recycle") == 0 || strcmp(name, "delete") == 0 ||
           strcmp(name, "clean") == 0 || strcmp(name, "dir") == 0 ||
           strcmp(name, "mkdir") == 0 || strcmp(name, "rmdir") == 0 ||
           strcmp(name, "move") == 0 || strcmp(name, "mv") == 0 ||
           strcmp(name, "copy") == 0 || strcmp(name, "cp") == 0 ||
           strcmp(name, "rename") == 0;
}

static u8 shell_root_mutation_denied(const char *command)
{
    if (shell_rex_command || strcmp(current_dir, ROOT_DIR) != 0 ||
        !shell_is_mutating_command(command)) return 0;

    SHELL_COLOR_ERR();
    kprintf("Permission denied: use 'rex %s' to modify the root directory\n", command);
    SHELL_COLOR_RESET();
    return 1;
}

static u8 shell_is_drift_chain(const char *line)
{
    return (strncmp(line, "var ", 4) == 0 || strncmp(line, "ask ", 4) == 0) &&
           shell_has_logical_operator(line);
}

static u8 shell_is_drift_statement(const char *line)
{
    return strncmp(line, "var ", 4) == 0 || strncmp(line, "ask ", 4) == 0 ||
           strncmp(line, "say ", 4) == 0 || strncmp(line, "if ", 3) == 0 ||
           strncmp(line, "if(", 3) == 0 || strncmp(line, "unless ", 7) == 0 ||
           strncmp(line, "unless(", 7) == 0 || strncmp(line, "while ", 6) == 0 ||
           strncmp(line, "while(", 6) == 0 || strncmp(line, "repeat ", 7) == 0 ||
           strncmp(line, "repeat(", 7) == 0 || strncmp(line, "for ", 4) == 0 ||
           strncmp(line, "for(", 4) == 0 || strncmp(line, "each ", 5) == 0 ||
           strncmp(line, "each(", 5) == 0 || strncmp(line, "when ", 5) == 0 ||
           strncmp(line, "when(", 5) == 0;
}

static int shell_execute_logical_line(const char *line) {
    char segment[SHELL_BUFFER_SIZE];
    int length = 0;
    int operator_kind = 0;
    int previous_status = 0;
    int found_operator = 0;
    int in_single_quote = 0;
    int in_double_quote = 0;
    u32 i;

    if (!shell_has_logical_operator(line)) return 0;

    for (i = 0;; i++) {
        char current = line[i];
        char next = line[i + 1];
        int boundary = 0;
        int next_operator = 0;

        if (current == '\'' && !in_double_quote) in_single_quote = !in_single_quote;
        if (current == '"' && !in_single_quote) in_double_quote = !in_double_quote;
        if (!in_single_quote && !in_double_quote) {
            if (current == ';') { boundary = 1; next_operator = 1; }
            else if (current == '&' && next == '&') { boundary = 1; next_operator = 2; }
            else if (current == '|' && next == '|') { boundary = 1; next_operator = 3; }
            else if (current == 0) boundary = 1;
        }

        if (boundary) {
            int should_execute = !found_operator || operator_kind == 1 ||
                                 (operator_kind == 2 && previous_status == 0) ||
                                 (operator_kind == 3 && previous_status != 0);
            int leading = 0;
            while (length > 0 && (segment[length - 1] == ' ' || segment[length - 1] == '\t')) length--;
            while (leading < length && (segment[leading] == ' ' || segment[leading] == '\t')) leading++;
            if (leading > 0) {
                memmove(segment, segment + leading, (size_t)(length - leading));
                length -= leading;
            }
            segment[length] = 0;
            if (length > 0 && should_execute) {
                previous_status = shell_execute_script_command(segment, NULL);
            }
            if (current == 0) break;
            found_operator = 1;
            operator_kind = next_operator;
            length = 0;
            if (next_operator == 2 || next_operator == 3) i++;
            continue;
        }

        if (length < (int)sizeof(segment) - 1) segment[length++] = current;
    }
    return found_operator;
}

static void shell_print_options_error(const char *command, const gsh_options_t *parsed) {
    SHELL_COLOR_ERR();
    if (parsed->error == GSH_OPTIONS_UNKNOWN_OPTION && parsed->error_short_name) {
        kprintf("%s: invalid option -- '%c'\n", command, parsed->error_short_name);
    } else if (parsed->error == GSH_OPTIONS_UNKNOWN_OPTION) {
        kprintf("%s: unrecognized option '--%s'\n", command, parsed->error_name);
    } else if (parsed->error == GSH_OPTIONS_MISSING_ARGUMENT && parsed->error_short_name) {
        kprintf("%s: option '-%c' requires an argument\n", command, parsed->error_short_name);
    } else if (parsed->error == GSH_OPTIONS_MISSING_ARGUMENT) {
        kprintf("%s: option '--%s' requires an argument\n", command, parsed->error_name);
    } else {
        kprintf("%s: invalid or too many arguments\n", command);
    }
    SHELL_COLOR_RESET();
}

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
    kprintf("[ %s @ galio ]:~ %s> ", host, dir);
    SHELL_COLOR_RESET();
    vga_get_hardware_cursor(&shell_input_origin_x, &shell_input_origin_y);
    input.cursor = 0;
    shell_rendered_input_len = 0;
    shell_cursor_draw();
}

static void shell_refresh_screen_state(void) {
    shell_cursor_restore();
    vga_clear();
    vga_set_color(0x0A);
    input.len = 0;
    input.cursor = 0;
    input.buffer[0] = 0;

    shell_print_prompt();
    shell_cursor_reset();
}

void shell_refresh_prompt(void) {
    shell_refresh_screen_state();
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
    for (u32 i = 0; i < shell_rendered_input_len; i++) {
        vga_write_cell(shell_input_origin_x + (int)i, shell_input_origin_y, ' ', 0x0F);
    }
    shell_rendered_input_len = 0;
    input.cursor = 0;
    vga_move_hardware_cursor(shell_input_origin_x, shell_input_origin_y);
    shell_cursor_draw();
}

static void shell_print_buffer(void);
static void shell_cancel_input(void)
{
    keyboard_clear_pending_input();
    shell_cursor_restore();
    vga_move_hardware_cursor(shell_input_origin_x + (int)input.len,
                             shell_input_origin_y);
    SHELL_COLOR_ERR();
    kprintf("^C\n");
    SHELL_COLOR_RESET();
    input.len = 0;
    input.cursor = 0;
    input.buffer[0] = 0;
    shell_print_prompt();
}

static void shell_print_history_entry(void) {
    if (history.index >= history.count) {
        shell_clear_line();
        input.len = 0;
        input.cursor = 0;
        input.buffer[0] = 0;
        return;
    }

    shell_clear_line();
    u32 idx = shell_history_array_index(history.index);
    strncpy(input.buffer, history.history[idx], SHELL_BUFFER_SIZE - 1);
    input.buffer[SHELL_BUFFER_SIZE - 1] = 0;
    input.len = strlen(input.buffer);
    if (input.len >= SHELL_BUFFER_SIZE) input.len = SHELL_BUFFER_SIZE - 1;
    input.cursor = input.len;
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
        input.cursor = 0;
        input.buffer[0] = 0;
    }
}

static void shell_print_buffer(void) {
    shell_cursor_restore();
    for (u32 i = input.len; i < shell_rendered_input_len; i++) {
        vga_write_cell(shell_input_origin_x + (int)i, shell_input_origin_y, ' ', 0x0F);
    }
    for (u32 i = 0; i < input.len; i++) {
        vga_write_cell(shell_input_origin_x + (int)i, shell_input_origin_y,
                       input.buffer[i], 0x0F);
    }
    shell_rendered_input_len = input.len;
    vga_move_hardware_cursor(shell_input_origin_x + (int)input.cursor, shell_input_origin_y);
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

static u8 shell_requires_rex_for_root_access(const char *path) {
    if (auth_is_authorized()) {
        return 0;
    }

    if (!path || *path == 0) return 0;
    char normalized[DIR_PATH_SIZE];
    if (!path_normalize(path, normalized, sizeof(normalized))) return 0;
    return strcmp(normalized, ".") == 0 || shell_is_direct_root_child(normalized);
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

    if (n == 1 && !bg && process_current() &&
        (process_current()->regs.cs & 3) == 0 &&
        (cmds[0][0] == '/' || (cmds[0][0] == '.' && cmds[0][1] == '/'))) {
        char pathbuf[PROCESS_PATH_MAX];
        char *name = cmds[0];
        while (*name == ' ') name++;
        if (name[0] == '/' || (name[0] == '.' && name[1] == '/')) {
            if (name[0] == '.' && name[1] == '/' && !vfs_find(name))
                path_resolve(current_dir, name + 2, pathbuf, sizeof(pathbuf));
            else
                path_resolve(current_dir, name, pathbuf, sizeof(pathbuf));
            u32 pid = process_create_user_elf(pathbuf);
            if (!pid) { SHELL_COLOR_ERR(); kprintf("Failed to execute: %s\n", pathbuf); SHELL_COLOR_RESET(); return; }
            shell_cursor_restore();
            vga_clear();
            SHELL_COLOR_RESET();
            sc_syscall1(SYS_WAITPID, (int)pid);
            return;
        }
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

            /* Resolve user paths against the shell's current directory. */
            char pathbuf[256];
            if (argv[0][0] == '/') {
                strncpy(pathbuf, argv[0], sizeof(pathbuf)-1);
                pathbuf[sizeof(pathbuf)-1] = 0;
            } else if (strncmp(argv[0], "./", 2) == 0 && !vfs_find(argv[0])) {
                path_resolve(current_dir, argv[0] + 2, pathbuf, sizeof(pathbuf));
            } else {
                path_resolve(current_dir, argv[0], pathbuf, sizeof(pathbuf));
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
    (void)privileged;
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
    shell_normalize_input();
    shell_last_status = 0;
    if (!shell_script_mode) shell_add_history(input.buffer);
    kprintf("\n");

    if (shell_root_mutation_denied(input.buffer)) {
        shell_last_status = 1;
        if (!shell_script_mode) shell_print_prompt();
        input.len = 0;
        return;
    }

    if (shell_is_drift_chain(input.buffer)) {
        if (!gsh_script_execute_line_with_command(input.buffer, shell_execute_script_command, NULL)) {
            shell_last_status = 1;
            SHELL_COLOR_ERR();
            kprintf("Drift: invalid statement\n");
            SHELL_COLOR_RESET();
        }
        if (!shell_script_mode) shell_print_prompt();
        input.len = 0;
        return;
    }

    if (shell_is_drift_statement(input.buffer)) {
        if (!gsh_script_execute_line_with_command(input.buffer, shell_execute_script_command, NULL)) {
            shell_last_status = 1;
            SHELL_COLOR_ERR();
            kprintf("Drift: invalid statement\n");
            SHELL_COLOR_RESET();
        }
        if (!shell_script_mode) shell_print_prompt();
        input.len = 0;
        return;
    }

    if (shell_drift_mode && strcmp(input.buffer, "exit") != 0 && strcmp(input.buffer, "quit") != 0) {
        if (!gsh_script_execute_line_with_command(input.buffer, shell_execute_script_command, NULL)) {
            shell_last_status = 1;
            SHELL_COLOR_ERR();
            kprintf("Drift: invalid statement\n");
            SHELL_COLOR_RESET();
        }
        if (!shell_script_mode) shell_print_prompt();
        input.len = 0;
        return;
    }
    if (shell_drift_mode && (strcmp(input.buffer, "exit") == 0 || strcmp(input.buffer, "quit") == 0)) {
        shell_drift_mode = 0;
        SHELL_COLOR_CMD();
        kprintf("Leaving Drift interpreter\n");
        SHELL_COLOR_RESET();
        if (!shell_script_mode) shell_print_prompt();
        input.len = 0;
        return;
    }

    if (shell_execute_logical_line(input.buffer)) {
        if (!shell_script_mode) shell_print_prompt();
        input.len = 0;
        return;
    }

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
                    if (!shell_script_mode) shell_print_prompt();
                    input.len = 0;
                    return;
                }
            }
            shell_run_pipeline(cmds, stages, bg);
            if (!shell_script_mode) shell_print_prompt();
            input.len = 0;
            return;
        } else if (stages == 1 && bg) {
            /* background single command: run as external */
            shell_run_pipeline(cmds, stages, bg);
            if (!shell_script_mode) shell_print_prompt();
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
                if (!shell_script_mode) shell_print_prompt();
                input.len = 0;
                return;
            }
            
            /* Verify password against kernel_auth credentials */
            if (!auth_verify_password(kernel_auth.username, password)) {
                SHELL_COLOR_ERR();
                kprintf("[REX] Access denied: Invalid password\n");
                SHELL_COLOR_RESET();
                if (!shell_script_mode) shell_print_prompt();
                input.len = 0;
                return;
            }
            
            /* Password correct - authorize for this session */
            auth_authorize();
            SHELL_COLOR_CMD();
            kprintf("[REX] Password accepted. Privileged mode enabled for this session.\n");
            SHELL_COLOR_RESET();
        }
        
        /* Now execute the privileged command (already authorized). Treat rex as a sudo-like
         * wrapper: any shell command that follows it is re-run through the normal shell parser
         * with the current user authorized for this session. */
        const char *cmd = input.buffer + 3;
        while (*cmd == ' ') cmd++;

        if (strlen(cmd) == 0) {
            SHELL_COLOR_ERR();
            kprintf("[REX] Usage: rex <command> [args...]\n");
            SHELL_COLOR_RESET();
            if (!shell_script_mode) shell_print_prompt();
            input.len = 0;
            return;
        }

        char privileged_cmd[SHELL_BUFFER_SIZE];
        strncpy(privileged_cmd, cmd, sizeof(privileged_cmd) - 1);
        privileged_cmd[sizeof(privileged_cmd) - 1] = 0;

        strncpy(input.buffer, privileged_cmd, SHELL_BUFFER_SIZE - 1);
        input.buffer[SHELL_BUFFER_SIZE - 1] = 0;
        input.len = strlen(input.buffer);
        shell_rex_command = 1;
        shell_execute_command();
        shell_rex_command = 0;
        return;
    } else if (strncmp(input.buffer, "run ", 4) == 0 || strncmp(input.buffer, "source ", 7) == 0) {
        const char *script_path = input.buffer + (input.buffer[0] == 'r' ? 4 : 7);
        while (*script_path == ' ') script_path++;
        if (*script_path == 0) {
            SHELL_COLOR_ERR();
            kprintf("Usage: run <script>\n");
            SHELL_COLOR_RESET();
        } else {
            shell_last_status = gsh_script_run_file(script_path, current_dir, shell_execute_script_command, NULL);
        }
    } else if (strcmp(input.buffer, "cpufreq") == 0 || strncmp(input.buffer, "cpufreq ", 8) == 0) {
        SHELL_COLOR_OUT();
        shell_cpufreq_command(input.buffer + 7);
        SHELL_COLOR_RESET();
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
    } else if (strncmp(input.buffer, "ifconfig", 8) == 0 &&
               (input.buffer[8] == ' ' || input.buffer[8] == '\0')) {
        SHELL_COLOR_OUT();
        shell_ifconfig_command(input.buffer + 8, current_dir);
        SHELL_COLOR_RESET();
    } else if (strncmp(input.buffer, "ip ", 3) == 0) {
        SHELL_COLOR_OUT();
        shell_ip_command(input.buffer + 3, current_dir);
        SHELL_COLOR_RESET();
    } else if (strcmp(input.buffer, "ip") == 0) {
        SHELL_COLOR_CMD();
        kprintf("Usage: ip <addr|route|renew|release> [interface]\n");
        SHELL_COLOR_RESET();
    } else if (strcmp(input.buffer, "wifi-list") == 0) {
        SHELL_COLOR_OUT();
        shell_wifi_list_command("", current_dir);
        SHELL_COLOR_RESET();
    } else if (strncmp(input.buffer, "syscall ", 8) == 0) {
        SHELL_COLOR_OUT();
        shell_syscall_command(input.buffer + 8, current_dir, shell_rex_command);
        SHELL_COLOR_RESET();
    } else if (strcmp(input.buffer, "syscall") == 0) {
        SHELL_COLOR_CMD();
        shell_syscall_command("", current_dir, shell_rex_command);
        SHELL_COLOR_RESET();
    } else if (strncmp(input.buffer, "SYS_", 4) == 0) {
        SHELL_COLOR_OUT();
        shell_syscall_command(input.buffer, current_dir, shell_rex_command);
        SHELL_COLOR_RESET();
    } else if (strncmp(input.buffer, "cpu-spike", 9) == 0 &&
               (input.buffer[9] == ' ' || input.buffer[9] == '\0')) {
        SHELL_COLOR_OUT();
        shell_spike_command(input.buffer + 9, current_dir);
        SHELL_COLOR_RESET();
    } else if (strncmp(input.buffer, "top ", 4) == 0) {
        SHELL_COLOR_OUT();
        shell_top_command(input.buffer + 4, current_dir);
        SHELL_COLOR_RESET();
    } else if (strcmp(input.buffer, "top") == 0) {
        SHELL_COLOR_OUT();
        shell_top_command("", current_dir);
        SHELL_COLOR_RESET();
    } else if (strncmp(input.buffer, "gc ", 3) == 0) {
        SHELL_COLOR_OUT();
        char *args = input.buffer + 3;
        while (*args == ' ') args++;
        char *source = args;
        while (*args && *args != ' ') args++;
        if (*args == 0) {
            kprintf("Usage: gc <source.c> <output>\n");
        } else {
            *args++ = 0;
            while (*args == ' ') args++;
            if (*args == 0) kprintf("Usage: gc <source.c> <output>\n");
            else {
                char source_path[VFS_MAX_PATH];
                char output_path[VFS_MAX_PATH];
                char *argv_gc[] = {"gc", source_path, output_path};
                if (strncmp(source, "./", 2) == 0 && !vfs_find(source))
                    path_resolve(current_dir, source + 2, source_path, sizeof(source_path));
                else
                    path_resolve(current_dir, source, source_path, sizeof(source_path));
                if (strncmp(args, "./", 2) == 0 && !vfs_find(args))
                    path_resolve(current_dir, args + 2, output_path, sizeof(output_path));
                else
                    path_resolve(current_dir, args, output_path, sizeof(output_path));
                cmd_gc(3, argv_gc);
            }
        }
        SHELL_COLOR_RESET();
    } else if (strcmp(input.buffer, "gc") == 0) {
        SHELL_COLOR_CMD();
        kprintf("Usage: gc <source.c> <output>\n");
        SHELL_COLOR_RESET();
    } else if (strncmp(input.buffer, "pkg ", 4) == 0) {
        SHELL_COLOR_OUT();
        shell_pkg_command(input.buffer + 4, current_dir);
        SHELL_COLOR_RESET();
    } else if (strcmp(input.buffer, "pkg") == 0) {
        SHELL_COLOR_CMD();
        kprintf("Usage: pkg <list|install|remove> [package_name]\n");
        SHELL_COLOR_RESET();
    } else if (strncmp(input.buffer, "where ", 6) == 0) {
        SHELL_COLOR_OUT();
        shell_where_command(input.buffer + 6, current_dir);
        SHELL_COLOR_RESET();
    } else if (strcmp(input.buffer, "where") == 0) {
        SHELL_COLOR_CMD();
        shell_where_command("", current_dir);
        SHELL_COLOR_RESET();
    } else if (strcmp(input.buffer, "date") == 0) {
        SHELL_COLOR_OUT();
        DateTime dt = kernel_time_get_datetime();
        static const char *day_names[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
        static const char *month_names[] = {"","Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
        /* compute day-of-week: Tomohiko Sakamoto */
        int y2 = dt.year; int m2 = dt.month; int d2 = dt.day;
        static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
        if (m2 < 3) y2--;
        int dow = (y2 + y2/4 - y2/100 + y2/400 + t[m2-1] + d2) % 7;
        kprintf("%s %s %02u %04u %02u:%02u:%02u\n",
                day_names[dow], month_names[dt.month], dt.day, dt.year,
                dt.hour, dt.minute, dt.second);
        SHELL_COLOR_RESET();
    } else if (strcmp(input.buffer, "time") == 0) {
        SHELL_COLOR_OUT();
        DateTime dt = kernel_time_get_datetime();
        kprintf("%02u:%02u:%02u\n", dt.hour, dt.minute, dt.second);
        SHELL_COLOR_RESET();
    } else if (strncmp(input.buffer, "clear", 5) == 0) {
        shell_cursor_restore();
        vga_clear();
        SHELL_COLOR_OUT();
    } else if (strncmp(input.buffer, "tree", 4) == 0) {
        const char *path = input.buffer + 4;
        while (*path == ' ') path++;

        char target[DIR_PATH_SIZE];
        if (*path == 0) {
            strncpy(target, current_dir, sizeof(target) - 1);
            target[sizeof(target) - 1] = 0;
        } else {
            shell_resolve_path(current_dir, path, target);
        }

        SHELL_COLOR_OUT();
        shell_tree_command(target);
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
        kprintf(" |  where    - Find a file or directory path (where <name>)       |\n");
        kprintf(" |__________________________________________________________________|\n");
        kprintf(" | recycle  - Move to recycle bin (usage: recycle <path1> [path2])  |\n");
        kprintf(" |__________________________________________________________________|\n");
        kprintf(" | clean    - Clean recycle bin (usage: clean rbin)                 |\n");
        kprintf(" |__________________________________________________________________|\n");
        kprintf(" |  net      - Networking commands (usage: net stat|scan|list|devices)     |\n");
        kprintf(" |________________________________________________________|\n");
        kprintf(" |  cpufreq  - CPU frequency policy and statistics                 |\n");
        kprintf(" |________________________________________________________|\n");
        kprintf(" |  top      - Live process monitor (Ctrl+C to stop)      |\n");
        kprintf(" |  cpu-spike - Live CPU usage graph (Ctrl+C to stop)     |\n");
        kprintf(" |________________________________________________________|\n");
        kprintf(" | delete   - Permanently delete (usage: delete <path1> [path2])|\n");
        kprintf(" |________________________________________________________|\n");
        kprintf(" | clear    - Clear the screen                            |\n");
        kprintf(" |________________________________________________________|\n");
        kprintf(" | echo     - Echo text (usage: echo <text>)              |\n");
        kprintf(" | run      - Execute a script (run <file>)               |\n");
        kprintf(" |________________________________________________________|\n");
        kprintf(" | uname    - Show system name                            |\n");
        kprintf(" | syscall  - Invoke a syscall (mutations require rex)   |\n");
        kprintf(" | wifi-list - Scan and list verified Wi-Fi networks      |\n");
        kprintf(" |___________________________ ____________________________|\n");
        kprintf(" |________________________________________________________|\n");
        kprintf(" | pwd      - Print current directory                     |\n");
        kprintf(" |________________________________________________________|\n");
        kprintf(" | goto     - Change directory (usage: goto <path>)       |\n");
        kprintf(" |________________________________________________________|\n");
        kprintf(" | back  - Go back to previous dir (usage: back [dirname])|\n");
        kprintf(" |_______________________________________________________ |\n");
        kprintf(" | rex      - Privileged command                            |\n");
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
    } else if (strcmp(input.buffer, "ls") == 0 || strncmp(input.buffer, "ls ", 3) == 0) {
        static const gsh_option_spec_t ls_specs[] = {
            {'l', "long", 0, 0},
            {'a', "all", 0, 0},
            {'h', "human-readable", 0, 0},
            {0, "help", 0, 0}
        };
        const char *args = input.buffer + 2;
        while (*args == ' ') args++;
        gsh_options_t parsed;
        if (gsh_parse_options(args, ls_specs, sizeof(ls_specs) / sizeof(ls_specs[0]), &parsed) != GSH_OPTIONS_OK) {
            shell_print_options_error("ls", &parsed);
        } else if (gsh_find_option(&parsed, 0, "help")) {
            SHELL_COLOR_CMD();
            kprintf("Usage: ls [OPTIONS] [PATH...]\n");
            kprintf("  -l, --long             Use long listing format\n");
            kprintf("  -a, --all              Show hidden entries\n");
            kprintf("  -h, --human-readable   Show human-readable sizes\n");
            SHELL_COLOR_RESET();
        } else {
            u8 show_all = gsh_find_option(&parsed, 'a', NULL) != NULL;
            u8 human_readable = gsh_find_option(&parsed, 'h', NULL) != NULL;
            u8 list_long = gsh_find_option(&parsed, 'l', NULL) != NULL;
            if (!list_long) list_long = gsh_find_option(&parsed, 0, "long") != NULL;
            for (u32 i = 0; i < parsed.positional_count; i++) {
                char target[DIR_PATH_SIZE];
                shell_resolve_path(current_dir, parsed.positional[i], target);
                SHELL_COLOR_OUT();
                vfs_listdir_options(target, show_all, human_readable);
                SHELL_COLOR_RESET();
            }
            if (parsed.positional_count == 0) {
                char target[DIR_PATH_SIZE];
                strncpy(target, current_dir, sizeof(target) - 1);
                target[sizeof(target) - 1] = 0;
                SHELL_COLOR_OUT();
                vfs_listdir_options(target, show_all, human_readable);
                SHELL_COLOR_RESET();
            }
            (void)list_long;
        }
    } else if (strncmp(input.buffer, "dir ", 4) == 0) {
        shell_dir_command(input.buffer + 4, current_dir, 0, 0);
    } else if (strcmp(input.buffer, "dir") == 0) {
        shell_dir_command("", current_dir, 0, 0);
    } else if (strcmp(input.buffer, "mkdir") == 0 || strncmp(input.buffer, "mkdir ", 6) == 0) {
        static const gsh_option_spec_t mkdir_specs[] = {
            {'p', "parents", 0, 0},
            {0, "help", 0, 0}
        };
        const char *args = input.buffer + 5;
        while (*args == ' ') args++;
        gsh_options_t parsed;
        if (gsh_parse_options(args, mkdir_specs, sizeof(mkdir_specs) / sizeof(mkdir_specs[0]), &parsed) != GSH_OPTIONS_OK) {
            shell_print_options_error("mkdir", &parsed);
        } else if (gsh_find_option(&parsed, 0, "help")) {
            SHELL_COLOR_CMD();
            kprintf("Usage: mkdir [OPTIONS] DIRECTORY...\n");
            kprintf("  -p, --parents   Create parent directories as needed\n");
            SHELL_COLOR_RESET();
        } else {
            char paths[512] = {0};
            for (u32 i = 0; i < parsed.positional_count; i++) {
                if (i) strncat(paths, " ", sizeof(paths) - strlen(paths) - 1);
                strncat(paths, parsed.positional[i], sizeof(paths) - strlen(paths) - 1);
            }
            shell_dir_command(paths, current_dir, 0, 0);
        }
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

        if (shell_requires_rex_for_root_access(fullpath)) {
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
    } else if (strcmp(input.buffer, "echo") == 0 || strncmp(input.buffer, "echo ", 5) == 0) {
        static const gsh_option_spec_t echo_specs[] = {
            {'n', "no-newline", 0, 0},
            {0, "help", 0, 0}
        };
        const char *args = input.buffer + 4;
        while (*args == ' ') args++;
        gsh_options_t parsed;
        if (gsh_parse_options(args, echo_specs, sizeof(echo_specs) / sizeof(echo_specs[0]), &parsed) != GSH_OPTIONS_OK) {
            shell_print_options_error("echo", &parsed);
        } else if (gsh_find_option(&parsed, 0, "help")) {
            SHELL_COLOR_CMD();
            kprintf("Usage: echo [OPTIONS] [STRING...]\n");
            kprintf("  -n, --no-newline   Do not print a trailing newline\n");
            SHELL_COLOR_RESET();
        } else {
            SHELL_COLOR_OUT();
            for (u32 i = 0; i < parsed.positional_count; i++) {
                if (i) kprintf(" ");
                kprintf("%s", parsed.positional[i]);
            }
            if (!gsh_find_option(&parsed, 'n', NULL)) kprintf("\n");
            SHELL_COLOR_RESET();
        }
    } else if (strncmp(input.buffer, "reboot", 6) == 0 || strncmp(input.buffer, "restart", 7) == 0) {
        SHELL_COLOR_OUT();
        kprintf("[POWER] Reboot command issued\n");
        power_system_reset();
        SHELL_COLOR_RESET();
    } else if (strncmp(input.buffer, "shutdown", 8) == 0 || strncmp(input.buffer, "shut-down", 9) == 0 || strncmp(input.buffer, "poweroff", 8) == 0) {
        SHELL_COLOR_OUT();
        kprintf("[POWER] Shutdown command issued\n");
        power_system_shutdown();
        SHELL_COLOR_RESET();
    } else if (strncmp(input.buffer, "uname", 5) == 0) {
        SHELL_COLOR_OUT();
        kprintf("Galio v1.0\n");
        SHELL_COLOR_RESET();
    } else if (strcmp(input.buffer, "refresh") == 0) {
        SHELL_COLOR_CMD();
        kprintf("[REFRESH] Reloading shell and VGA state...\n");
        SHELL_COLOR_RESET();
        shell_refresh_screen_state();
    } else if (input.len > 0 &&
               (input.buffer[0] == '/' ||
                (input.buffer[0] == '.' && input.buffer[1] == '/'))) {
        char script_path[DIR_PATH_SIZE];
        if (shell_is_gh_script_command(input.buffer, script_path, sizeof(script_path))) {
            shell_last_status = gsh_script_run_file(script_path, current_dir, shell_execute_script_command, NULL);
        } else {
            char external_cmds[1][256];
            strncpy(external_cmds[0], input.buffer, sizeof(external_cmds[0]) - 1);
            external_cmds[0][sizeof(external_cmds[0]) - 1] = 0;
            shell_run_pipeline(external_cmds, 1, 0);
        }
    } else if (strncmp(input.buffer, "drift ", 6) == 0) {
        const char *script_path = input.buffer + 6;
        while (*script_path == ' ' || *script_path == '\t') script_path++;
        if (*script_path == 0) {
            SHELL_COLOR_CMD();
            kprintf("Drift interpreter: use drift <script.df>\n");
            SHELL_COLOR_RESET();
        } else {
            shell_last_status = gsh_script_run_file(script_path, current_dir, shell_execute_script_command, NULL);
        }
    } else if (strcmp(input.buffer, "drift") == 0) {
        shell_drift_mode = 1;
        SHELL_COLOR_CMD();
        kprintf("Entering Drift interpreter. Type exit or quit to return.\n");
        SHELL_COLOR_RESET();
    } else if (strncmp(input.buffer, "gsh ", 4) == 0) {
        if (!gsh_script_execute_line_with_command(input.buffer + 4, shell_execute_script_command, NULL)) {
            shell_last_status = 1;
            SHELL_COLOR_ERR();
            kprintf("gsh: invalid Drift statement\n");
            SHELL_COLOR_RESET();
        }
    } else if (strcmp(input.buffer, "gsh") == 0) {
        SHELL_COLOR_CMD();
        kprintf("GSH interpreter: use gsh <Drift statement>\n");
        SHELL_COLOR_RESET();
    } else if (input.len > 0) {
        if (shell_script_mode) {
            shell_last_status = 1;
            SHELL_COLOR_ERR();
            kprintf("Unknown command: %s\nType 'help' for available commands\n", input.buffer);
            SHELL_COLOR_RESET();
        } else if (!gsh_script_execute_line_with_command(input.buffer, shell_execute_script_command, NULL)) {
            shell_last_status = 1;
            SHELL_COLOR_ERR();
            kprintf("Unknown command: %s\nType 'help' for available commands\n", input.buffer);
            SHELL_COLOR_RESET();
        }
    }

    if (!shell_script_mode) shell_print_prompt();
    input.len = 0;
}

int shell_execute_script_command(const char *command, void *context) {
    u8 previous_mode = shell_script_mode;
    u32 length;
    (void)context;
    if (!command) return -1;
    length = strlen(command);
    if (length == 0) return 0;
    if (length >= SHELL_BUFFER_SIZE) length = SHELL_BUFFER_SIZE - 1;
    memcpy(input.buffer, command, length);
    input.buffer[length] = 0;
    input.len = length;
    input.cursor = length;
    shell_script_mode = 1;
    shell_execute_command();
    shell_script_mode = previous_mode;
    return shell_should_exit ? 1 : shell_last_status;
}

/* Poll keyboard for input (no IRQs) */
static void shell_poll_keyboard(void) {
    u8 scancode;
    u8 is_pressed;
    u8 extended;

    if (keyboard_take_ctrl_c()) {
        shell_cancel_input();
        return;
    }

    if (keyboard_read_shell_event(&scancode, &is_pressed, &extended)) {
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
            } else if (scancode == 0x4B) {
                if (input.cursor > 0) {
                    input.cursor--;
                    shell_print_buffer();
                }
                return;
            } else if (scancode == 0x4D) {
                if (input.cursor < input.len) {
                    input.cursor++;
                    shell_print_buffer();
                }
                return;
            }
            return;
        }

        u8 c = scancode_to_ascii(scancode);
        if (c == 0) {
            return;
        }

        if (c == '\b') {
            if (input.cursor > 0) {
                shell_cursor_restore();
                for (u32 i = input.cursor; i < input.len; i++) {
                    input.buffer[i - 1] = input.buffer[i];
                }
                input.len--;
                input.cursor--;
                input.buffer[input.len] = 0;
                shell_print_buffer();
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
                for (u32 i = input.len; i > input.cursor; i--) {
                    input.buffer[i] = input.buffer[i - 1];
                }
                input.buffer[input.cursor] = c;
                input.len++;
                input.cursor++;
                input.buffer[input.len] = 0;
                shell_print_buffer();
            }
        }
        return;
    }
}

void shell_run(void) {
    process_accounting_set_idle(0);
    input.len = 0;
    input.cursor = 0;
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
    enable_interrupts();

    while (!shell_should_exit) {
        shell_poll_keyboard();
        shell_poll_mouse();
        for (volatile int i = 0; i < 100; i++);
    }

    shell_cursor_restore();
    shell_cursor_drawn = 0;
    vga_disable_hardware_cursor();  /* Disable hardware cursor; UI manages its own */
    vga_clear();
}
