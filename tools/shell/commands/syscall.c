#include "common.h"
#include "kprintf.h"
#include "string.h"
#include "user_syscall.h"

#define SHELL_SYSCALL_TOKEN_MAX 128
#define SHELL_SYSCALL_ARG_MAX 256

static int shell_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static char *shell_skip_ws(char *s) {
    if (!s) return NULL;
    while (*s && shell_is_space(*s)) {
        s++;
    }
    return s;
}

static char *shell_next_token(char *src, char *dst, u32 dst_size) {
    if (!src || !dst || dst_size == 0) {
        return NULL;
    }

    src = shell_skip_ws(src);
    if (!*src) {
        return NULL;
    }

    u32 i = 0;
    while (*src && !shell_is_space(*src) && i + 1 < dst_size) {
        dst[i++] = *src++;
    }
    dst[i] = 0;

    return shell_skip_ws(src);
}

static void shell_print_usage(const char *cmd) {
    if (strcmp(cmd, "pid") == 0) {
        kprintf("Usage: syscall pid\n");
    } else if (strcmp(cmd, "uid") == 0) {
        kprintf("Usage: syscall uid\n");
    } else if (strcmp(cmd, "gid") == 0) {
        kprintf("Usage: syscall gid\n");
    } else if (strcmp(cmd, "time") == 0) {
        kprintf("Usage: syscall time\n");
    } else if (strcmp(cmd, "suspend") == 0) {
        kprintf("Usage: syscall suspend\n");
    } else if (strcmp(cmd, "fork") == 0) {
        kprintf("Usage: syscall fork\n");
    } else if (strcmp(cmd, "pipe") == 0) {
        kprintf("Usage: syscall pipe\n");
    } else if (strcmp(cmd, "dup") == 0) {
        kprintf("Usage: syscall dup <fd>\n");
    } else if (strcmp(cmd, "mmap") == 0) {
        kprintf("Usage: syscall mmap <length>\n");
    } else if (strcmp(cmd, "brk") == 0) {
        kprintf("Usage: syscall brk [address]\n");
    } else if (strcmp(cmd, "wait") == 0) {
        kprintf("Usage: syscall wait [pid]\n");
    } else if (strcmp(cmd, "open") == 0) {
        kprintf("Usage: syscall open <path> [flags]\n");
    } else if (strcmp(cmd, "read") == 0) {
        kprintf("Usage: syscall read <fd> <count>\n");
    } else if (strcmp(cmd, "close") == 0) {
        kprintf("Usage: syscall close <fd>\n");
    } else if (strcmp(cmd, "seek") == 0) {
        kprintf("Usage: syscall seek <fd> <offset> <whence>\n");
    } else if (strcmp(cmd, "stat") == 0) {
        kprintf("Usage: syscall stat <path>\n");
    } else if (strcmp(cmd, "exec") == 0) {
        kprintf("Usage: syscall exec <path>\n");
    } else if (strcmp(cmd, "execve") == 0) {
        kprintf("Usage: syscall execve <path> [arg ...]\n");
    } else {
        kprintf("Usage: syscall <pid|uid|gid|time|suspend|fork|pipe|dup|mmap|brk|wait|open|read|close|seek|stat|exec|execve>\n");
    }
}

static int shell_parse_i64(const char *text, i64 *out_value) {
    if (!text || !out_value) {
        return 0;
    }

    const char *p = text;
    while (*p && shell_is_space(*p)) {
        p++;
    }
    if (*p == 0) {
        return 0;
    }

    i64 value = 0;
    i64 sign = 1;
    if (*p == '+') {
        p++;
    } else if (*p == '-') {
        sign = -1;
        p++;
    }

    u8 hex = 0;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        hex = 1;
        p += 2;
    }

    while (*p && !shell_is_space(*p)) {
        char c = *p;
        u32 digit;
        if (hex) {
            if (c >= '0' && c <= '9') digit = (u32)(c - '0');
            else if (c >= 'a' && c <= 'f') digit = (u32)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') digit = (u32)(c - 'A' + 10);
            else return 0;
            value = (value * 16) + (i64)digit;
        } else {
            if (c < '0' || c > '9') return 0;
            value = (value * 10) + (i64)(c - '0');
        }
        p++;
    }

    *out_value = value * sign;
    return 1;
}

static int shell_parse_u64(const char *text, u64 *out_value) {
    if (!text || !out_value) {
        return 0;
    }

    const char *p = text;
    while (*p && shell_is_space(*p)) {
        p++;
    }
    if (*p == 0) {
        return 0;
    }

    u64 value = 0;
    u8 hex = 0;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        hex = 1;
        p += 2;
    }

    while (*p && !shell_is_space(*p)) {
        char c = *p;
        u32 digit;
        if (hex) {
            if (c >= '0' && c <= '9') digit = (u32)(c - '0');
            else if (c >= 'a' && c <= 'f') digit = (u32)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') digit = (u32)(c - 'A' + 10);
            else return 0;
            value = (value << 4) | digit;
        } else {
            if (c < '0' || c > '9') return 0;
            value = value * 10u + (u64)(c - '0');
        }
        p++;
    }

    *out_value = value;
    return 1;
}

static void shell_epoch_to_datetime(u32 epoch, u16 *year, u8 *month, u8 *day, u8 *hour, u8 *minute, u8 *second) {
    if (!year || !month || !day || !hour || !minute || !second) {
        return;
    }

    u32 seconds = epoch;
    *second = (u8)(seconds % 60u);
    seconds /= 60u;
    *minute = (u8)(seconds % 60u);
    seconds /= 60u;
    *hour = (u8)(seconds % 24u);
    u32 days = seconds / 24u;

    u16 y = 1970;
    while (1) {
        u32 year_days = 365u + ((y % 4u == 0u && (y % 100u != 0u || y % 400u == 0u)) ? 1u : 0u);
        if (days >= year_days) {
            days -= year_days;
            y++;
        } else {
            break;
        }
    }
    *year = y;

    u8 m = 1;
    while (1) {
        u32 month_days = 31u;
        if (m == 2) month_days = (y % 4u == 0u && (y % 100u != 0u || y % 400u == 0u)) ? 29u : 28u;
        else if (m == 4 || m == 6 || m == 9 || m == 11) month_days = 30u;
        if (days >= month_days) {
            days -= month_days;
            m++;
        } else {
            break;
        }
    }
    *month = m;
    *day = (u8)(days + 1u);
}

static const char *shell_syscall_error_name(i32 code) {
    if (code == -1) return "System call failed";
    if (code == -2) return "No such file or directory";
    if (code == -9) return "Bad file descriptor";
    if (code == -12) return "Out of memory";
    if (code == -22) return "Invalid argument";
    if (code == -29) return "Illegal seek";
    if (code == -5) return "I/O error";
    if (code == -71) return "No child processes";
    return "System call failed";
}

u8 shell_syscall_command(const char *args, const char *current_dir) {
    (void)current_dir;
    if (!args || *args == 0) {
        shell_print_usage("syscall");
        return 0;
    }

    char buffer[SHELL_SYSCALL_ARG_MAX];
    strncpy(buffer, args, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = 0;

    char *cmd = shell_skip_ws(buffer);
    if (!cmd || *cmd == 0) {
        shell_print_usage("syscall");
        return 0;
    }

    char *rest = cmd;
    while (*rest && !shell_is_space(*rest)) {
        rest++;
    }
    if (*rest) {
        *rest++ = 0;
    }
    rest = shell_skip_ws(rest);

    if (strcmp(cmd, "pid") == 0) {
        if (*rest) {
            shell_print_usage(cmd);
            return 0;
        }
        kprintf("PID: %d\n", sys_getpid());
        return 1;
    }
    if (strcmp(cmd, "uid") == 0) {
        if (*rest) {
            shell_print_usage(cmd);
            return 0;
        }
        kprintf("UID: %u\n", sys_getuid());
        return 1;
    }
    if (strcmp(cmd, "gid") == 0) {
        if (*rest) {
            shell_print_usage(cmd);
            return 0;
        }
        kprintf("GID: %u\n", sys_getgid());
        return 1;
    }
    if (strcmp(cmd, "time") == 0) {
        if (*rest) {
            shell_print_usage(cmd);
            return 0;
        }
        u32 epoch = sys_time();
        u16 year = 1970; u8 month = 1; u8 day = 1; u8 hour = 0; u8 minute = 0; u8 second = 0;
        shell_epoch_to_datetime(epoch, &year, &month, &day, &hour, &minute, &second);
        kprintf("Epoch: %u\nDate: %04u-%02u-%02u %02u:%02u:%02u\n", epoch, year, month, day, hour, minute, second);
        return 1;
    }
    if (strcmp(cmd, "fork") == 0) {
        if (*rest) {
            shell_print_usage(cmd);
            return 0;
        }
        i32 child = sys_fork();
        if (child < 0) {
            kprintf("fork: %s\n", shell_syscall_error_name(child));
            return 0;
        }
        if (child == 0) {
            kprintf("Child PID: %d\n", sys_getpid());
        } else {
            kprintf("Child PID: %d\n", child);
        }
        return 1;
    }
    if (strcmp(cmd, "pipe") == 0) {
        if (*rest) {
            shell_print_usage(cmd);
            return 0;
        }
        int fds[2] = {0, 0};
        int rc = sys_pipe(fds);
        if (rc < 0) {
            kprintf("pipe: %s\n", shell_syscall_error_name(rc));
            return 0;
        }
        kprintf("read_fd: %d\nwrite_fd: %d\n", fds[0], fds[1]);
        return 1;
    }
    if (strcmp(cmd, "dup") == 0) {
        char fd_token[SHELL_SYSCALL_TOKEN_MAX];
        char *next = shell_next_token(rest, fd_token, sizeof(fd_token));
        if (!fd_token[0]) {
            kprintf("Invalid file descriptor: %s\n", fd_token[0] ? fd_token : "<missing>");
            shell_print_usage(cmd);
            return 0;
        }
        i64 fd_value;
        if (!shell_parse_i64(fd_token, &fd_value) || fd_value < 0) {
            kprintf("Invalid file descriptor: %s\n", fd_token);
            shell_print_usage(cmd);
            return 0;
        }
        if (next && *next) {
            shell_print_usage(cmd);
            return 0;
        }
        int new_fd = sys_dup((int)fd_value);
        if (new_fd < 0) {
            kprintf("dup: %s\n", shell_syscall_error_name(new_fd));
            return 0;
        }
        kprintf("New FD: %d\n", new_fd);
        return 1;
    }
    if (strcmp(cmd, "mmap") == 0) {
        char len_token[SHELL_SYSCALL_TOKEN_MAX];
        char *next = shell_next_token(rest, len_token, sizeof(len_token));
        if (!len_token[0]) {
            kprintf("mmap: length must be greater than zero\n");
            shell_print_usage(cmd);
            return 0;
        }
        u64 length;
        if (!shell_parse_u64(len_token, &length) || length == 0 || length > 0x10000000u) {
            kprintf("mmap: length must be greater than zero and not unreasonable\n");
            shell_print_usage(cmd);
            return 0;
        }
        if (next && *next) {
            shell_print_usage(cmd);
            return 0;
        }
        void *ptr = sys_mmap(NULL, (unsigned long)length, 3, 1, -1, 0);
        if ((uintptr_t)ptr == (uintptr_t)-1) {
            kprintf("mmap: %s\n", shell_syscall_error_name(-12));
            return 0;
        }
        kprintf("Mapped address: 0x%llx\nLength: %llu\n", (unsigned long long)(uintptr_t)ptr, (unsigned long long)length);
        return 1;
    }
    if (strcmp(cmd, "brk") == 0) {
        if (!rest || !*rest) {
            void *res = sys_brk(NULL);
            if ((uintptr_t)res == (uintptr_t)-1) {
                kprintf("brk: %s\n", shell_syscall_error_name(-22));
                return 0;
            }
            kprintf("Current program break: 0x%llx\n", (unsigned long long)(uintptr_t)res);
            return 1;
        }
        char addr_token[SHELL_SYSCALL_TOKEN_MAX];
        char *next = shell_next_token(rest, addr_token, sizeof(addr_token));
        if (!addr_token[0]) {
            shell_print_usage(cmd);
            return 0;
        }
        u64 parsed = 0;
        if (!shell_parse_u64(addr_token, &parsed)) {
            kprintf("brk: invalid address\n");
            shell_print_usage(cmd);
            return 0;
        }
        if (next && *next) {
            shell_print_usage(cmd);
            return 0;
        }
        void *res = sys_brk((void *)parsed);
        if ((uintptr_t)res == (uintptr_t)-1) {
            kprintf("brk: %s\n", shell_syscall_error_name(-22));
            return 0;
        }
        kprintf("New program break: 0x%llx\n", (unsigned long long)(uintptr_t)res);
        return 1;
    }
    if (strcmp(cmd, "wait") == 0) {
        if (!rest || !*rest) {
            i32 rc = sys_waitpid(-1);
            if (rc < 0) {
                kprintf("wait: %s\n", shell_syscall_error_name(rc));
                return 0;
            }
            kprintf("Process %d exited\n", rc);
            return 1;
        }
        char pid_token[SHELL_SYSCALL_TOKEN_MAX];
        char *next = shell_next_token(rest, pid_token, sizeof(pid_token));
        if (!pid_token[0]) {
            shell_print_usage(cmd);
            return 0;
        }
        i64 pid;
        if (!shell_parse_i64(pid_token, &pid)) {
            kprintf("wait: invalid pid\n");
            shell_print_usage(cmd);
            return 0;
        }
        if (next && *next) {
            shell_print_usage(cmd);
            return 0;
        }
        i32 rc = sys_waitpid((int)pid);
        if (rc < 0) {
            kprintf("wait: %s\n", shell_syscall_error_name(rc));
            return 0;
        }
        kprintf("Process %d exited\n", rc);
        return 1;
    }
    if (strcmp(cmd, "open") == 0) {
        char path_token[SHELL_SYSCALL_TOKEN_MAX];
        char *after_path = shell_next_token(rest, path_token, sizeof(path_token));
        if (!path_token[0]) {
            shell_print_usage(cmd);
            return 0;
        }
        int flags = 0;
        if (after_path && *after_path) {
            char flags_token[SHELL_SYSCALL_TOKEN_MAX];
            char *after_flags = shell_next_token(after_path, flags_token, sizeof(flags_token));
            if (!flags_token[0]) {
                shell_print_usage(cmd);
                return 0;
            }
            i64 value;
            if (!shell_parse_i64(flags_token, &value) || value < 0) {
                kprintf("open: invalid flags\n");
                return 0;
            }
            flags = (int)value;
            if (after_flags && *after_flags) {
                shell_print_usage(cmd);
                return 0;
            }
        }
        int fd = sys_open(path_token, flags);
        if (fd < 0) {
            kprintf("open: %s\n", shell_syscall_error_name(fd));
            return 0;
        }
        kprintf("FD: %d\n", fd);
        return 1;
    }
    if (strcmp(cmd, "read") == 0) {
        char fd_token[SHELL_SYSCALL_TOKEN_MAX];
        char *after_fd = shell_next_token(rest, fd_token, sizeof(fd_token));
        if (!fd_token[0]) {
            shell_print_usage(cmd);
            return 0;
        }
        i64 fd_value;
        if (!shell_parse_i64(fd_token, &fd_value) || fd_value < 0) {
            kprintf("read: invalid file descriptor\n");
            return 0;
        }
        char count_token[SHELL_SYSCALL_TOKEN_MAX];
        char *after_count = shell_next_token(after_fd, count_token, sizeof(count_token));
        if (!count_token[0]) {
            shell_print_usage(cmd);
            return 0;
        }
        u64 count_value;
        if (!shell_parse_u64(count_token, &count_value) || count_value == 0 || count_value > 4096u) {
            kprintf("read: count must be greater than zero and not exceed 4096\n");
            return 0;
        }
        if (after_count && *after_count) {
            shell_print_usage(cmd);
            return 0;
        }

        char buffer_local[4096];
        i32 rc = sys_read((int)fd_value, buffer_local, (unsigned long)count_value);
        if (rc < 0) {
            kprintf("read: %s\n", shell_syscall_error_name(rc));
            return 0;
        }
        buffer_local[rc < (i32)sizeof(buffer_local) ? rc : (i32)sizeof(buffer_local)] = 0;
        kprintf("Read %d bytes:\n", rc);
        if (rc > 0) {
            kprintf("%.*s\n", rc, buffer_local);
        }
        return 1;
    }
    if (strcmp(cmd, "close") == 0) {
        char fd_token[SHELL_SYSCALL_TOKEN_MAX];
        char *next = shell_next_token(rest, fd_token, sizeof(fd_token));
        if (!fd_token[0]) {
            shell_print_usage(cmd);
            return 0;
        }
        i64 fd_value;
        if (!shell_parse_i64(fd_token, &fd_value) || fd_value < 0) {
            kprintf("close: invalid file descriptor\n");
            return 0;
        }
        if (next && *next) {
            shell_print_usage(cmd);
            return 0;
        }
        int rc = sys_close((int)fd_value);
        if (rc < 0) {
            kprintf("close: %s\n", shell_syscall_error_name(rc));
            return 0;
        }
        kprintf("FD %d closed\n", (int)fd_value);
        return 1;
    }
    if (strcmp(cmd, "seek") == 0) {
        char fd_token[SHELL_SYSCALL_TOKEN_MAX];
        char *after_fd = shell_next_token(rest, fd_token, sizeof(fd_token));
        if (!fd_token[0]) {
            shell_print_usage(cmd);
            return 0;
        }
        i64 fd_value;
        if (!shell_parse_i64(fd_token, &fd_value) || fd_value < 0) {
            kprintf("seek: invalid file descriptor\n");
            return 0;
        }
        char off_token[SHELL_SYSCALL_TOKEN_MAX];
        char *after_off = shell_next_token(after_fd, off_token, sizeof(off_token));
        if (!off_token[0]) {
            shell_print_usage(cmd);
            return 0;
        }
        i64 off_value;
        if (!shell_parse_i64(off_token, &off_value)) {
            kprintf("seek: invalid offset\n");
            return 0;
        }
        char whence_token[SHELL_SYSCALL_TOKEN_MAX];
        char *after_whence = shell_next_token(after_off, whence_token, sizeof(whence_token));
        if (!whence_token[0]) {
            shell_print_usage(cmd);
            return 0;
        }
        i64 whence_value;
        if (!shell_parse_i64(whence_token, &whence_value) || (whence_value != 0 && whence_value != 1 && whence_value != 2)) {
            kprintf("seek: invalid whence: %s\n", whence_token);
            return 0;
        }
        if (after_whence && *after_whence) {
            shell_print_usage(cmd);
            return 0;
        }
        long new_offset = sys_lseek((int)fd_value, (long)off_value, (int)whence_value);
        if (new_offset < 0) {
            kprintf("seek: %s\n", shell_syscall_error_name((i32)new_offset));
            return 0;
        }
        kprintf("New offset: %ld\n", new_offset);
        return 1;
    }
    if (strcmp(cmd, "stat") == 0) {
        char path_token[SHELL_SYSCALL_TOKEN_MAX];
        char *next = shell_next_token(rest, path_token, sizeof(path_token));
        if (!path_token[0]) {
            shell_print_usage(cmd);
            return 0;
        }
        if (next && *next) {
            shell_print_usage(cmd);
            return 0;
        }
        struct {
            u32 mode;
            u32 size;
            u32 blocks;
            u32 atime;
            u32 mtime;
            u32 ctime;
        } st = {0};
        int rc = sys_stat(path_token, &st);
        if (rc < 0) {
            kprintf("stat: %s\n", shell_syscall_error_name(rc));
            return 0;
        }
        kprintf("Path: %s\nType: %s\nSize: %u bytes\nMode: %04o\n", path_token, (st.mode & 0x4000) ? "dir" : ((st.mode & 0x8000) ? "file" : "N/A"), st.size, st.mode & 0x0fff);
        kprintf("Inode: N/A\nUID: 1000\nGID: 1000\n");
        return 1;
    }
    if (strcmp(cmd, "exec") == 0) {
        char path_token[SHELL_SYSCALL_TOKEN_MAX];
        char *next = shell_next_token(rest, path_token, sizeof(path_token));
        if (!path_token[0]) {
            shell_print_usage(cmd);
            return 0;
        }
        if (next && *next) {
            shell_print_usage(cmd);
            return 0;
        }
        int rc = sys_exec(path_token);
        if (rc < 0) {
            kprintf("exec: %s\n", shell_syscall_error_name(rc));
            return 0;
        }
        return 1;
    }
    if (strcmp(cmd, "execve") == 0) {
        char path_token[SHELL_SYSCALL_TOKEN_MAX];
        char *next = shell_next_token(rest, path_token, sizeof(path_token));
        if (!path_token[0]) {
            shell_print_usage(cmd);
            return 0;
        }
        char *argv[32];
        u32 argc = 0;
        argv[argc++] = path_token;
        while (next && *next) {
            char arg_token[SHELL_SYSCALL_TOKEN_MAX];
            next = shell_next_token(next, arg_token, sizeof(arg_token));
            if (!arg_token[0]) {
                break;
            }
            if (argc >= 31) {
                kprintf("execve: too many arguments\n");
                return 0;
            }
            argv[argc++] = arg_token;
        }
        argv[argc] = NULL;
        int rc = sys_execve(path_token, argv, NULL);
        if (rc < 0) {
            kprintf("execve: %s\n", shell_syscall_error_name(rc));
            return 0;
        }
        return 1;
    }

    kprintf("Unknown syscall command: %s\n", cmd);
    return 0;
}
