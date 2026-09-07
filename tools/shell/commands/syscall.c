#include "syscall_cmd.h"
#include "kprintf.h"
#include "string.h"
#include "user_syscall.h"
#include "process.h"

static int syscall_command_number(const char *text) {
    int value = 0;
    if (!text || !*text) return -1;
    while (*text >= '0' && *text <= '9') value = value * 10 + (*text++ - '0');
    return *text ? -1 : value;
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
           strcmp(name, "number") == 0;
}

static int syscall_name_number(const char *name) {
    static const struct { const char *name; int number; } calls[] = {
        {"read", SYS_READ}, {"write", SYS_WRITE}, {"open", SYS_OPEN},
        {"close", SYS_CLOSE}, {"stat", SYS_STAT}, {"fstat", SYS_FSTAT},
        {"lstat", SYS_LSTAT}, {"poll", SYS_POLL}, {"lseek", SYS_LSEEK},
        {"mmap", SYS_MMAP}, {"mprotect", SYS_MPROTECT}, {"munmap", SYS_MUNMAP},
        {"brk", SYS_BRK}, {"sigaction", SYS_RT_SIGACTION},
        {"sigprocmask", SYS_RT_SIGPROCMASK}, {"sigreturn", SYS_RT_SIGRETURN},
        {"ioctl", SYS_IOCTL}, {"pread64", SYS_PREAD64}, {"pwrite64", SYS_PWRITE64},
        {"readv", SYS_READV}, {"writev", SYS_WRITEV}, {"access", SYS_ACCESS},
        {"pipe", SYS_PIPE}, {"select", SYS_SELECT}, {"yield", SYS_SCHED_YIELD},
        {"dup", SYS_DUP}, {"dup2", SYS_DUP2}, {"pause", SYS_PAUSE},
        {"nanosleep", SYS_NANOSLEEP}, {"getpid", SYS_GETPID}, {"socket", SYS_SOCKET},
        {"connect", SYS_CONNECT}, {"accept", SYS_ACCEPT}, {"sendto", SYS_SENDTO},
        {"recvfrom", SYS_RECVFROM}, {"sendmsg", SYS_SENDMSG}, {"recvmsg", SYS_RECVMSG},
        {"shutdown", SYS_SHUTDOWN}, {"bind", SYS_BIND}, {"listen", SYS_LISTEN},
        {"getsockname", SYS_GETSOCKNAME}, {"getpeername", SYS_GETPEERNAME},
        {"socketpair", SYS_SOCKETPAIR}, {"setsockopt", SYS_SETSOCKOPT},
        {"getsockopt", SYS_GETSOCKOPT}, {"clone", SYS_CLONE}, {"fork", SYS_FORK},
        {"vfork", SYS_VFORK}, {"execve", SYS_EXECVE}, {"exec", SYS_EXEC},
        {"exit", SYS_EXIT}, {"wait4", SYS_WAIT4}, {"waitpid", SYS_WAITPID},
        {"wait", SYS_WAIT}, {"kill", SYS_KILL}, {"uname", SYS_UNAME},
        {"semget", SYS_SEMGET}, {"semop", SYS_SEMOP}, {"semctl", SYS_SEMCTL},
        {"shmdt", SYS_SHMDT}, {"shmget", SYS_SHMGET}, {"shmat", SYS_SHMAT},
        {"shmctl", SYS_SHMCTL}, {"gettimeofday", SYS_GETTIMEOFDAY},
        {"getuid", SYS_GETUID}, {"geteuid", SYS_GETEUID}, {"getegid", SYS_GETEGID},
        {"getgid", SYS_GETGID}, {"getppid", SYS_GETPPID}, {"getcwd", SYS_GETCWD},
        {"chdir", SYS_CHDIR}, {"readlink", SYS_READLINK},
        {"clock_gettime", SYS_CLOCK_GETTIME}, {"rt_sigaction", SYS_RT_SIGACTION},
        {"rt_sigprocmask", SYS_RT_SIGPROCMASK}, {"rt_sigreturn", SYS_RT_SIGRETURN},
        {"sched_yield", SYS_SCHED_YIELD}, {"ioctl2", SYS_IOCTL2},
        {"sysinfo", SYS_SYSINFO},
        {"mkdir", SYS_MKDIR},
        {"rmdir", SYS_RMDIR},
        {"unlink", SYS_UNLINK},
        {"rename", SYS_RENAME},
        {"chmod", SYS_CHMOD},
        {"fsync", SYS_FSYNC},
        {"time", SYS_TIME}, {"sleep", SYS_SLEEP}, {"mmap2", SYS_MMAP2}
    };
    for (u32 i = 0; i < sizeof(calls) / sizeof(calls[0]); i++) {
        if (strcmp(name, calls[i].name) == 0) return calls[i].number;
    }
    return -1;
}

u8 shell_syscall_command(const char *args, const char *current_dir, u8 privileged) {
    (void)current_dir;
    if (!args || !*args) {
        kprintf("Usage: syscall <getpid|getppid|uid|gid|time|yield|sleep|uname|exit|fork|wait|pipe|dup|close|number>\n");
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
        int number = syscall_command_number(value);
        if (number < 0) {
            kprintf("Usage: syscall number <syscall-number>\n");
            return 0;
        }
        long rc = galio_syscall(number, 0, 0, 0, 0, 0);
        kprintf("syscall %d = %ld\n", number, rc);
        return rc >= 0;
    }
    int number = syscall_name_number(name);
    if (number >= 0) {
        long rc = galio_syscall(number, 0, 0, 0, 0, 0);
        kprintf("SYS_%s (%d) = %ld\n", name, number, rc);
        return rc >= 0;
    }
    kprintf("syscall: unknown command '%s'\n", name);
    return 0;
}
