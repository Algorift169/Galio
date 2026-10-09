#include "syscall_cmd.h"
#include "kprintf.h"
#include "string.h"
#include "user_syscall.h"
#include "process.h"
#include "syscall_catalog.h"
#include "srver/server.h"

static int syscall_command_number(const char *text) {
    int value = 0;
    if (!text || !*text) return -1;
    while (*text >= '0' && *text <= '9') value = value * 10 + (*text++ - '0');
    return *text ? -1 : value;
}

u8 shell_wait_command(const char *args) {
    if (!args || !*args) {
        kprintf("Usage: wait <seconds>\n");
        return 0;
    }
    int seconds = syscall_command_number(args);
    if (seconds < 0) {
        kprintf("Usage: wait <seconds>\n");
        return 0;
    }
    int result = syscall_wait_time((u64)seconds);
    if (result != 0) {
        kprintf("wait: seconds out of range\n");
        return 0;
    }
    return 1;
}

static u8 syscall_next_token(const char **cursor, char *output, u32 capacity) {
    u32 length = 0;
    while (**cursor == ' ') (*cursor)++;
    if (!**cursor || capacity == 0) return 0;
    u8 quoted = **cursor == '"';
    if (quoted) (*cursor)++;
    while (**cursor && (quoted ? **cursor != '"' : **cursor != ' ')) {
        if (length + 1 >= capacity) return 0;
        output[length++] = *(*cursor)++;
    }
    if (quoted) {
        if (**cursor != '"') return 0;
        (*cursor)++;
    }
    output[length] = 0;
    while (**cursor == ' ') (*cursor)++;
    return length != 0;
}

static u8 syscall_parse_arguments(const char *text, long arguments[6], u32 *count) {
    *count = 0;
    while (*text) {
        while (*text == ' ') text++;
        if (!*text) break;
        if (*count == 6) return 0;

        u8 negative = 0;
        if (*text == '-' || *text == '+') {
            negative = *text == '-';
            text++;
        }
        u32 base = 10;
        if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
            base = 16;
            text += 2;
        }
        if (!*text) return 0;

        u64 value = 0;
        u8 digits = 0;
        while (*text && *text != ' ') {
            u32 digit;
            if (*text >= '0' && *text <= '9') digit = (u32)(*text - '0');
            else if (base == 16 && *text >= 'a' && *text <= 'f') digit = (u32)(*text - 'a' + 10);
            else if (base == 16 && *text >= 'A' && *text <= 'F') digit = (u32)(*text - 'A' + 10);
            else return 0;
            if (digit >= base || value > (0x7FFFFFFFFFFFFFFFULL - digit) / base) return 0;
            value = value * base + digit;
            digits = 1;
            text++;
        }
        if (!digits) return 0;
        arguments[*count] = negative ? -(long)value : (long)value;
        (*count)++;
    }
    return 1;
}

static u32 syscall_catalog_argument_count(const syscall_catalog_entry_t *entry) {
    u32 count = 0;
    u8 in_argument = 0;
    for (const char *cursor = entry->arguments; *cursor; cursor++) {
        if (*cursor == ' ') {
            in_argument = 0;
        } else if (!in_argument) {
            count++;
            in_argument = 1;
        }
    }
    return count;
}

static const syscall_catalog_entry_t *syscall_find_named(const char *name) {
    if (strcmp(name, "uid") == 0) name = "getuid";
    else if (strcmp(name, "gid") == 0) name = "getgid";
    else if (strcmp(name, "yield") == 0) name = "sched_yield";
    else if (strcmp(name, "sigaction") == 0) name = "rt_sigaction";
    else if (strcmp(name, "sigprocmask") == 0) name = "rt_sigprocmask";
    return syscall_catalog_find(name);
}

static const syscall_catalog_entry_t *syscall_normalized_lookup(char *name) {
    for (u32 i = 0; name[i]; i++) {
        if (name[i] >= 'A' && name[i] <= 'Z') name[i] = (char)(name[i] - 'A' + 'a');
    }
    if (strncmp(name, "sys_", 4) == 0) {
        u32 length = strlen(name);
        for (u32 i = 0; i + 4 <= length; i++) name[i] = name[i + 4];
    }
    return syscall_find_named(name);
}

static u8 syscall_run_raw(const syscall_catalog_entry_t *entry, const char *text) {
    long arguments[6] = {0, 0, 0, 0, 0, 0};
    u32 argument_count;
    if (!syscall_parse_arguments(text, arguments, &argument_count)) {
        kprintf("syscall %s: expected up to six decimal/hex arguments\n", entry->name);
        return 0;
    }
    if (entry->status == SYSCALL_STATUS_UNDISPATCHED) {
        kprintf("SYS_%s (%u): %s\n", entry->name, entry->number,
                syscall_status_name(entry->status));
        return 0;
    }
    u32 required = syscall_catalog_argument_count(entry);
    if (argument_count != required) {
        kprintf("Usage: syscall raw %s", entry->name);
        if (entry->arguments[0]) kprintf(" <%s>", entry->arguments);
        kprintf("\n");
        return 0;
    }
    long result = galio_syscall6(entry->number, arguments[0], arguments[1],
                                 arguments[2], arguments[3], arguments[4],
                                 arguments[5]);
        kprintf("SYS_%s (%u) = %ld [%s]\n", entry->name, entry->number, result,
            syscall_status_name(entry->status));
    return result >= 0;
}

static u8 syscall_requires_rex(const char *name) {
    return strcmp(name, "write") == 0 || strcmp(name, "exit") == 0 || strcmp(name, "fork") == 0 ||
           strcmp(name, "exec") == 0 || strcmp(name, "wait") == 0 ||
           strcmp(name, "open") == 0 || strcmp(name, "close") == 0 ||
           strcmp(name, "pipe") == 0 || strcmp(name, "dup") == 0 ||
           strcmp(name, "dup2") == 0 || strcmp(name, "chdir") == 0 ||
           strcmp(name, "mmap") == 0 || strcmp(name, "munmap") == 0 ||
           strcmp(name, "brk") == 0 || strcmp(name, "kill") == 0 ||
           strcmp(name, "mprotect") == 0 || strcmp(name, "pwrite64") == 0 ||
           strcmp(name, "writev") == 0 || strcmp(name, "sigaction") == 0 ||
           strcmp(name, "sigprocmask") == 0 || strcmp(name, "bind") == 0 ||
           strcmp(name, "listen") == 0 || strcmp(name, "connect") == 0 ||
           strcmp(name, "sendto") == 0 || strcmp(name, "sendmsg") == 0 ||
           strcmp(name, "shutdown") == 0 || strcmp(name, "semget") == 0 ||
           strcmp(name, "semop") == 0 || strcmp(name, "semctl") == 0 ||
           strcmp(name, "shmget") == 0 || strcmp(name, "shmat") == 0 ||
           strcmp(name, "shmdt") == 0 || strcmp(name, "shmctl") == 0 ||
           strcmp(name, "socket") == 0 || strcmp(name, "socketpair") == 0 ||
           strcmp(name, "clone") == 0 || strcmp(name, "vfork") == 0 ||
           strcmp(name, "number") == 0 || strcmp(name, "raw") == 0 ||
           strcmp(name, "unlink") == 0 || strcmp(name, "rmdir") == 0 ||
           strcmp(name, "rename") == 0 || strcmp(name, "chmod") == 0 ||
           strcmp(name, "fsync") == 0 || strcmp(name, "execve") == 0 ||
           strcmp(name, "wait4") == 0 || strcmp(name, "mmap2") == 0 ||
           strcmp(name, "gui_set_background") == 0;
}

static int syscall_name_number(const char *name) {
    const syscall_catalog_entry_t *entry = syscall_find_named(name);
    return entry ? (int)entry->number : -1;
}

u8 shell_syscall_command(const char *args, const char *current_dir, u8 privileged) {
    (void)current_dir;
    if (!args || !*args) {
        kprintf("Usage: syscall list [implemented|unsupported] | status <name> | <name> [args...]\n");
        return 0;
    }

    char name[32];
    u32 name_len = 0;
    while (args[name_len] && args[name_len] != ' ' && name_len + 1 < sizeof(name)) {
        name[name_len] = args[name_len];
        name_len++;
    }
    name[name_len] = 0;
    if (name_len > 4 && name[0] == 'S' && name[1] == 'Y' && name[2] == 'S' && name[3] == '_') {
        for (u32 i = 4; i < name_len; i++) name[i - 4] = name[i];
        name_len -= 4;
        name[name_len] = 0;
    }
    for (u32 i = 0; i < name_len; i++) {
        if (name[i] >= 'A' && name[i] <= 'Z') name[i] = (char)(name[i] - 'A' + 'a');
    }
    if (name_len > 1 && name[name_len - 2] == '(' && name[name_len - 1] == ')') {
        name[name_len - 2] = 0;
        name_len -= 2;
    }
    while (args[name_len] == ' ') name_len++;
    const char *value = args + name_len;

    if (strcmp(name, "list") == 0) {
        u8 implemented_only = strcmp(value, "implemented") == 0;
        u8 unsupported_only = strcmp(value, "unsupported") == 0;
        for (u32 i = 0; i < syscall_catalog_count(); i++) {
            const syscall_catalog_entry_t *entry = syscall_catalog_at(i);
            if (implemented_only && entry->status != SYSCALL_STATUS_IMPLEMENTED) continue;
            if (unsupported_only && entry->status == SYSCALL_STATUS_IMPLEMENTED) continue;
                kprintf("%s %u [%s] %s\n", entry->name, entry->number,
                    syscall_status_name(entry->status),
                    entry->arguments[0] ? entry->arguments : "(no arguments)");
        }
        return 1;
    }

    if (strcmp(name, "status") == 0) {
        char query[32];
        u32 length = 0;
        while (value[length] && value[length] != ' ' && length + 1 < sizeof(query)) {
            query[length] = value[length];
            length++;
        }
        query[length] = 0;
        const syscall_catalog_entry_t *entry = syscall_normalized_lookup(query);
        if (!entry) {
            kprintf("syscall: unknown name '%s'\n", query);
            return 0;
        }
        kprintf("SYS_%s = %u (%s); args: %s\n", entry->name, entry->number,
                syscall_status_name(entry->status),
                entry->arguments[0] ? entry->arguments : "none");
        return 1;
    }

    if (strcmp(name, "raw") == 0) {
        char target[32];
        u32 length = 0;
        while (value[length] && value[length] != ' ' && length + 1 < sizeof(target)) {
            target[length] = value[length];
            length++;
        }
        target[length] = 0;
        while (value[length] == ' ') length++;
        const syscall_catalog_entry_t *entry = syscall_normalized_lookup(target);
        if (!entry) {
            kprintf("syscall raw: unknown canonical name '%s'\n", target);
            return 0;
        }
        if (!privileged) {
            kprintf("syscall raw: permission denied; use 'rex syscall raw %s'\n", target);
            return 0;
        }
        return syscall_run_raw(entry, value + length);
    }

    if (syscall_requires_rex(name) && !privileged) {
        kprintf("syscall %s: permission denied; use 'rex syscall %s'\n", name, args);
        return 0;
    }
    if (strcmp(name, "getpid") == 0) {
        kprintf("SYS_GETPID = %d\n", sys_getpid());
        return 1;
    }
    if (strcmp(name, "getppid") == 0) {
        kprintf("SYS_GETPPID = %ld\n", galio_syscall(SYS_GETPPID, 0, 0, 0, 0, 0));
        return 1;
    }
    if (strcmp(name, "uid") == 0) {
        kprintf("SYS_GETUID = %d\n", sys_getuid());
        return 1;
    }
    if (strcmp(name, "gid") == 0) {
        kprintf("SYS_GETGID = %d\n", sys_getgid());
        return 1;
    }
    if (strcmp(name, "time") == 0) {
        kprintf("SYS_TIME = %ld\n", sys_time());
        return 1;
    }
    if (strcmp(name, "gettimeofday") == 0) {
        struct timeval time_value;
        int result = (int)galio_syscall(SYS_GETTIMEOFDAY, (long)&time_value, 0, 0, 0, 0);
        if (result == 0) {
            kprintf("SYS_GETTIMEOFDAY = %u.%06u\n", time_value.tv_sec, time_value.tv_usec);
        } else {
            kprintf("SYS_GETTIMEOFDAY = %d\n", result);
        }
        return result == 0;
    }
    if (strcmp(name, "clock_gettime") == 0) {
        int clock_id = syscall_command_number(value);
        if (clock_id < 0) {
            kprintf("Usage: syscall clock_gettime <clock_id>\n");
            return 0;
        }
        struct galio_timespec time_value;
        long result = galio_syscall(SYS_CLOCK_GETTIME, clock_id, (long)&time_value, 0, 0, 0);
        if (result == 0) {
            kprintf("SYS_CLOCK_GETTIME = %lld.%09lld\n",
                    (long long)time_value.tv_sec, (long long)time_value.tv_nsec);
        } else {
            kprintf("SYS_CLOCK_GETTIME = %ld\n", result);
        }
        return result == 0;
    }
    if (strcmp(name, "sysinfo") == 0) {
        galio_sysinfo_t info;
        int result = sys_sysinfo(&info);
        if (result == 0) {
            kprintf("SYS_SYSINFO uptime=%llu ram=%llu free=%llu processes=%u cpus=%u\n",
                    (unsigned long long)info.uptime,
                    (unsigned long long)info.totalram,
                    (unsigned long long)info.freeram, info.procs, info.cpus);
        } else {
            kprintf("SYS_SYSINFO = %d\n", result);
        }
        return result == 0;
    }
    if (strcmp(name, "nanosleep") == 0) {
        int milliseconds = syscall_command_number(value);
        if (milliseconds < 0) {
            kprintf("Usage: syscall nanosleep <milliseconds>\n");
            return 0;
        }
        struct galio_timespec request = {
            .tv_sec = milliseconds / 1000,
            .tv_nsec = (milliseconds % 1000) * 1000000LL
        };
        struct galio_timespec remaining;
        long result = galio_syscall(SYS_NANOSLEEP, (long)&request, (long)&remaining, 0, 0, 0);
        kprintf("SYS_NANOSLEEP(%d ms) = %ld\n", milliseconds, result);
        return result == 0;
    }
    if (strcmp(name, "gui_connect") == 0) {
        int client_id = sys_gui_connect();
        kprintf("SYS_GUI_CONNECT = %d\n", client_id);
        return client_id > 0;
    }
    if (strcmp(name, "gui_info") == 0) {
        galio_gui_info_t info;
        int result = sys_gui_info(&info);
        if (result == 0) {
            kprintf("SYS_GUI_INFO = %ux%u running=%u frames=%u\n",
                    info.width, info.height, info.running, info.frame_count);
        } else {
            kprintf("SYS_GUI_INFO = %d\n", result);
        }
        return result == 0;
    }
    if (strcmp(name, "gui_window_create") == 0) {
        char title[DISPLAY_SERVER_TITLE_LEN];
        char value_text[4][16];
        const char *cursor = value;
        int values[4];
        if (!syscall_next_token(&cursor, title, sizeof(title))) {
            kprintf("Usage: syscall gui_window_create <title> <x> <y> <width> <height>\n");
            return 0;
        }
        for (u32 i = 0; i < 4; i++) {
            if (!syscall_next_token(&cursor, value_text[i], sizeof(value_text[i])) ||
                (values[i] = syscall_command_number(value_text[i])) < 0) {
                kprintf("Usage: syscall gui_window_create <title> <x> <y> <width> <height>\n");
                return 0;
            }
        }
        int window_id = sys_gui_window_create(title, values[0], values[1],
                                              (u32)values[2], (u32)values[3]);
        kprintf("SYS_GUI_WINDOW_CREATE = %d\n", window_id);
        return window_id > 0;
    }
    if (strcmp(name, "gui_window_destroy") == 0 ||
        strcmp(name, "gui_window_focus") == 0) {
        int window_id = syscall_command_number(value);
        if (window_id < 0) {
            kprintf("Usage: syscall %s <window_id>\n", name);
            return 0;
        }
        int result = strcmp(name, "gui_window_destroy") == 0 ?
            sys_gui_window_destroy((u32)window_id) : sys_gui_window_focus((u32)window_id);
        kprintf("SYS_%s = %d\n", name, result);
        return result == 0;
    }
    if (strcmp(name, "gui_window_move") == 0) {
        char tokens[3][16];
        const char *cursor = value;
        int values[3];
        for (u32 i = 0; i < 3; i++) {
            if (!syscall_next_token(&cursor, tokens[i], sizeof(tokens[i])) ||
                (values[i] = syscall_command_number(tokens[i])) < 0) {
                kprintf("Usage: syscall gui_window_move <window_id> <x> <y>\n");
                return 0;
            }
        }
        int result = sys_gui_window_move((u32)values[0], values[1], values[2]);
        kprintf("SYS_GUI_WINDOW_MOVE = %d\n", result);
        return result == 0;
    }
    if (strcmp(name, "gui_surface_create") == 0) {
        char tokens[3][16];
        const char *cursor = value;
        int values[3];
        for (u32 i = 0; i < 3; i++) {
            if (!syscall_next_token(&cursor, tokens[i], sizeof(tokens[i])) ||
                (values[i] = syscall_command_number(tokens[i])) < 0) {
                kprintf("Usage: syscall gui_surface_create <window_id> <width> <height>\n");
                return 0;
            }
        }
        int surface_id = sys_gui_surface_create((u32)values[0], (u32)values[1], (u32)values[2]);
        kprintf("SYS_GUI_SURFACE_CREATE = %d\n", surface_id);
        return surface_id > 0;
    }
    if (strcmp(name, "gui_set_background") == 0) {
        int color = syscall_command_number(value);
        if (color < 0) {
            kprintf("Usage: rex syscall gui_set_background <rgb_color>\n");
            return 0;
        }
        int result = sys_gui_set_background((u32)color);
        kprintf("SYS_GUI_SET_BACKGROUND = %d\n", result);
        return result == 0;
    }
    if (strcmp(name, "gui_cycle_window") == 0) {
        int direction = syscall_command_number(value);
        if (direction == 0) {
            kprintf("Usage: syscall gui_cycle_window <-1|1>\n");
            return 0;
        }
        int result = sys_gui_cycle_window(direction < 0 ? -1 : 1);
        kprintf("SYS_GUI_CYCLE_WINDOW = %d\n", result);
        return result == 0;
    }
    if (strcmp(name, "open") == 0) {
        char path[PROCESS_PATH_MAX];
        const char *cursor = value;
        if (!syscall_next_token(&cursor, path, sizeof(path))) {
            kprintf("Usage: rex syscall open <path> [flags]\n");
            return 0;
        }
        int flags = *cursor ? syscall_command_number(cursor) : 0;
        if (flags < 0) {
            kprintf("Usage: rex syscall open <path> [flags]\n");
            return 0;
        }
        int fd = sys_open(path, flags);
        kprintf("SYS_OPEN(%s, %d) = %d\n", path, flags, fd);
        return fd >= 0;
    }
    if (strcmp(name, "read") == 0) {
        char fd_text[16];
        char count_text[16];
        const char *cursor = value;
        if (!syscall_next_token(&cursor, fd_text, sizeof(fd_text)) ||
            !syscall_next_token(&cursor, count_text, sizeof(count_text))) {
            kprintf("Usage: rex syscall read <fd> <count>\n");
            return 0;
        }
        int fd = syscall_command_number(fd_text);
        int count = syscall_command_number(count_text);
        if (fd < 0 || count < 0 || count > 255) {
            kprintf("syscall read: count must be 0..255\n");
            return 0;
        }
        char buffer[256];
        int result = sys_read(fd, buffer, (u32)count);
        if (result < 0) {
            kprintf("SYS_READ = %d\n", result);
            return 0;
        }
        kprintf("SYS_READ = %d: ", result);
        for (int i = 0; i < result; i++) {
            char character = buffer[i];
            kprintf("%c", character >= 32 || character == '\n' || character == '\t' ? character : '.');
        }
        kprintf("\n");
        return 1;
    }
    if (strcmp(name, "write") == 0) {
        char fd_text[16];
        const char *cursor = value;
        if (!syscall_next_token(&cursor, fd_text, sizeof(fd_text)) || !*cursor) {
            kprintf("Usage: rex syscall write <fd> <text>\n");
            return 0;
        }
        int fd = syscall_command_number(fd_text);
        if (fd < 0) return 0;
        u32 length = strlen(cursor);
        if (length >= 2 && cursor[0] == '"' && cursor[length - 1] == '"') {
            cursor++;
            length -= 2;
        }
        int result = sys_write(fd, cursor, length);
        kprintf("\nSYS_WRITE = %d\n", result);
        return result >= 0;
    }
    if (strcmp(name, "stat") == 0 || strcmp(name, "fstat") == 0) {
        struct stat info;
        int result;
        if (strcmp(name, "stat") == 0) {
            char path[PROCESS_PATH_MAX];
            const char *cursor = value;
            if (!syscall_next_token(&cursor, path, sizeof(path))) {
                kprintf("Usage: syscall stat <path>\n");
                return 0;
            }
            result = sys_stat(path, &info);
        } else {
            int fd = syscall_command_number(value);
            if (fd < 0) {
                kprintf("Usage: syscall fstat <fd>\n");
                return 0;
            }
            result = (int)galio_syscall(SYS_FSTAT, fd, (long)&info, 0, 0, 0);
        }
        if (result != 0) {
            kprintf("SYS_%s = %d\n", name, result);
            return 0;
        }
        kprintf("SYS_%s: mode=0x%x uid=%u gid=%u size=%llu\n", name,
                info.st_mode, info.st_uid, info.st_gid,
                (unsigned long long)info.st_size);
        return 1;
    }
    if (strcmp(name, "access") == 0) {
        char path[PROCESS_PATH_MAX];
        const char *cursor = value;
        if (!syscall_next_token(&cursor, path, sizeof(path))) {
            kprintf("Usage: syscall access <path> [mode]\n");
            return 0;
        }
        int mode = *cursor ? syscall_command_number(cursor) : 0;
        if (mode < 0) return 0;
        int result = (int)galio_syscall(SYS_ACCESS, (long)path, mode, 0, 0, 0);
        kprintf("SYS_ACCESS(%s, %d) = %d\n", path, mode, result);
        return result == 0;
    }
    if (strcmp(name, "chdir") == 0) {
        char path[PROCESS_PATH_MAX];
        const char *cursor = value;
        if (!syscall_next_token(&cursor, path, sizeof(path))) {
            kprintf("Usage: rex syscall chdir <path>\n");
            return 0;
        }
        int result = sys_chdir(path);
        kprintf("SYS_CHDIR(%s) = %d\n", path, result);
        return result == 0;
    }
    if (strcmp(name, "exec") == 0 || strcmp(name, "execve") == 0) {
        char path[PROCESS_PATH_MAX];
        const char *cursor = value;
        if (!syscall_next_token(&cursor, path, sizeof(path))) {
            kprintf("Usage: rex syscall %s <path>\n", name);
            return 0;
        }
        if (strcmp(name, "exec") == 0) {
            int result = sys_exec(path);
            kprintf("SYS_EXEC(%s) = %d\n", path, result);
            return result == 0;
        }
        char *arguments[2] = { path, NULL };
        char *environment[1] = { NULL };
        int result = sys_execve(path, arguments, environment);
        kprintf("SYS_EXECVE(%s) = %d\n", path, result);
        return result == 0;
    }
    if (strcmp(name, "getcwd") == 0) {
        char path[PROCESS_PATH_MAX];
        int result = sys_getcwd(path, sizeof(path));
        kprintf("SYS_GETCWD = %d (%s)\n", result, result >= 0 ? path : "");
        return result >= 0;
    }
    if (strcmp(name, "readlink") == 0) {
        char path[PROCESS_PATH_MAX];
        char target[PROCESS_PATH_MAX];
        const char *cursor = value;
        if (!syscall_next_token(&cursor, path, sizeof(path))) {
            kprintf("Usage: syscall readlink <path>\n");
            return 0;
        }
        int result = (int)galio_syscall(SYS_READLINK, (long)path,
                                        (long)target, sizeof(target), 0, 0);
        if (result < 0) kprintf("SYS_READLINK = %d\n", result);
        else {
            if ((u32)result < sizeof(target)) target[result] = 0;
            kprintf("SYS_READLINK = %d (%s)\n", result, target);
        }
        return result >= 0;
    }
    if (strcmp(name, "yield") == 0) {
        kprintf("SYS_SCHED_YIELD = %d\n", sys_sched_yield());
        return 1;
    }
    if (strcmp(name, "sleep") == 0) {
        int milliseconds = syscall_command_number(value);
        if (milliseconds < 0) {
            kprintf("Usage: syscall sleep <milliseconds>\n");
            return 0;
        }
        kprintf("SYS_SLEEP(%d) = %d\n", milliseconds, sys_sleep((u32)milliseconds));
        return 1;
    }
    if (strcmp(name, "uname") == 0) {
        struct utsname info;
        int rc = (int)galio_syscall(SYS_UNAME, (long)&info, 0, 0, 0, 0);
        kprintf("SYS_UNAME = %d (%s %s %s)\n", rc, info.sysname, info.release, info.machine);
        return rc == 0;
    }
    if (strcmp(name, "mkdir") == 0) {
        if (!value || !*value) {
            kprintf("Usage: rex syscall mkdir <path>\n");
            return 0;
        }
        int rc = sys_mkdir(value, 0755);
        kprintf("SYS_MKDIR(%s) = %d\n", value, rc);
        return rc == 0;
    }
    if (strcmp(name, "rmdir") == 0) {
        if (!value || !*value) {
            kprintf("Usage: rex syscall rmdir <path>\n");
            return 0;
        }
        int rc = sys_rmdir(value);
        kprintf("SYS_RMDIR(%s) = %d\n", value, rc);
        return rc == 0;
    }
    if (strcmp(name, "unlink") == 0) {
        if (!value || !*value) {
            kprintf("Usage: rex syscall unlink <path>\n");
            return 0;
        }
        int rc = sys_unlink(value);
        kprintf("SYS_UNLINK(%s) = %d\n", value, rc);
        return rc == 0;
    }
    if (strcmp(name, "rename") == 0) {
        char old_path[PROCESS_PATH_MAX];
        char new_path[PROCESS_PATH_MAX];
        u32 split = 0;
        while (value[split] && value[split] != ' ') split++;
        if (!value[split]) {
            kprintf("Usage: rex syscall rename <old> <new>\n");
            return 0;
        }
        if (split >= sizeof(old_path)) split = sizeof(old_path) - 1;
        strncpy(old_path, value, split);
        old_path[split] = 0;
        while (value[split] == ' ') split++;
        strncpy(new_path, value + split, sizeof(new_path) - 1);
        new_path[sizeof(new_path) - 1] = 0;
        int rc = sys_rename(old_path, new_path);
        kprintf("SYS_RENAME(%s, %s) = %d\n", old_path, new_path, rc);
        return rc == 0;
    }
    if (strcmp(name, "chmod") == 0) {
        char path[PROCESS_PATH_MAX];
        int mode = -1;
        u32 split = 0;
        while (value[split] && value[split] != ' ') split++;
        if (!value[split]) {
            kprintf("Usage: rex syscall chmod <mode> <path>\n");
            return 0;
        }
        mode = syscall_command_number(value);
        while (value[split] == ' ') split++;
        strncpy(path, value + split, sizeof(path) - 1);
        path[sizeof(path) - 1] = 0;
        if (mode < 0) {
            kprintf("Usage: rex syscall chmod <mode> <path>\n");
            return 0;
        }
        int rc = sys_chmod(path, (u32)mode);
        kprintf("SYS_CHMOD(%s, %d) = %d\n", path, mode, rc);
        return rc == 0;
    }
    if (strcmp(name, "fsync") == 0) {
        int fd = syscall_command_number(value);
        if (fd < 0) {
            kprintf("Usage: rex syscall fsync <fd>\n");
            return 0;
        }
        int rc = sys_fsync(fd);
        kprintf("SYS_FSYNC(%d) = %d\n", fd, rc);
        return rc == 0;
    }
    if (strcmp(name, "exit") == 0) {
        int status = syscall_command_number(value);
        if (status < 0) status = 0;
        kprintf("SYS_EXIT(%d)\n", status);
        galio_syscall(SYS_EXIT, status, 0, 0, 0, 0);
        return 1;
    }
    if (strcmp(name, "fork") == 0) {
        kprintf("SYS_FORK = %d\n", sys_fork());
        return 1;
    }
    if (strcmp(name, "wait") == 0) {
        int pid = syscall_command_number(value);
        if (pid < 0) {
            kprintf("Usage: rex syscall wait <pid>\n");
            return 0;
        }
        kprintf("SYS_WAITPID(%d) = %d\n", pid, sys_waitpid(pid));
        return 1;
    }
    if (strcmp(name, "pipe") == 0) {
        int fds[2];
        int rc = sys_pipe(fds);
        kprintf("SYS_PIPE = %d\n", rc);
        if (rc == 0) kprintf("read=%d write=%d\n", fds[0], fds[1]);
        return rc == 0;
    }
    if (strcmp(name, "dup") == 0 || strcmp(name, "close") == 0) {
        int fd = syscall_command_number(value);
        if (fd < 0) {
            kprintf("Usage: rex syscall %s <fd>\n", name);
            return 0;
        }
        int rc = strcmp(name, "dup") == 0 ? sys_dup(fd) : sys_close(fd);
        kprintf("SYS_%s(%d) = %d\n", strcmp(name, "dup") == 0 ? "DUP" : "CLOSE", fd, rc);
        return rc >= 0;
    }
    if (strcmp(name, "number") == 0) {
        char number_text[16];
        const char *cursor = value;
        if (!syscall_next_token(&cursor, number_text, sizeof(number_text))) {
            kprintf("Usage: rex syscall number <syscall-number> [arg1 ... arg6]\n");
            return 0;
        }
        int number = syscall_command_number(number_text);
        if (number < 0) {
            kprintf("Usage: rex syscall number <syscall-number> [arg1 ... arg6]\n");
            return 0;
        }
        const syscall_catalog_entry_t *entry = syscall_catalog_find_number((u32)number);
        if (!entry) {
            kprintf("syscall number %d: not in the canonical catalog\n", number);
            return 0;
        }
        return syscall_run_raw(entry, cursor);
    }
    int number = syscall_name_number(name);
    if (number >= 0) {
        const syscall_catalog_entry_t *entry = syscall_find_named(name);
        return syscall_run_raw(entry, value);
    }
    kprintf("syscall: unknown command '%s'\n", name);
    return 0;
}
