#include "common.h"
#include "kprintf.h"
#include "string.h"
#include "user_syscall.h"

u8 shell_syscall_command(const char *args, const char *current_dir) {
    (void)current_dir;
    if (!args || *args == 0) {
        kprintf("Usage: syscall <pid|uid|gid|time|fork|pipe|dup|mmap|brk|wait|open|read|close|seek|stat|exec>\n");
        return 0;
    }

    if (strcmp(args, "pid") == 0) {
        kprintf("pid = %d\n", sys_getpid());
        return 1;
    }
    if (strcmp(args, "uid") == 0) {
        kprintf("uid = %d\n", sys_getuid());
        return 1;
    }
    if (strcmp(args, "gid") == 0) {
        kprintf("gid = %d\n", sys_getgid());
        return 1;
    }
    if (strcmp(args, "time") == 0) {
        kprintf("time = %ld\n", sys_time());
        return 1;
    }
    if (strcmp(args, "fork") == 0) {
        kprintf("fork() -> %d\n", sys_fork());
        return 1;
    }
    if (strcmp(args, "pipe") == 0) {
        int fds[2] = {0, 0};
        kprintf("pipe() -> %d\n", sys_pipe(fds));
        return 1;
    }
    if (strcmp(args, "dup") == 0) {
        kprintf("dup(0) -> %d\n", sys_dup(0));
        return 1;
    }
    if (strcmp(args, "mmap") == 0) {
        void *p = sys_mmap(NULL, 4096, 3, 1, -1, 0);
        kprintf("mmap() -> %p\n", p);
        return 1;
    }
    if (strcmp(args, "brk") == 0) {
        void *p = sys_brk((void *)0x40001000);
        kprintf("brk() -> %p\n", p);
        return 1;
    }
    if (strcmp(args, "wait") == 0) {
        kprintf("waitpid(-1) -> %d\n", sys_waitpid(-1));
        return 1;
    }
    if (strcmp(args, "open") == 0) {
        kprintf("open(./) -> %d\n", sys_open("./", 0));
        return 1;
    }
    if (strcmp(args, "read") == 0) {
        char buf[32] = {0};
        kprintf("read(0, ...) -> %d\n", sys_read(0, buf, sizeof(buf)));
        return 1;
    }
    if (strcmp(args, "close") == 0) {
        kprintf("close(0) -> %d\n", sys_close(0));
        return 1;
    }
    if (strcmp(args, "seek") == 0) {
        kprintf("lseek(0, 0, 0) -> %ld\n", sys_lseek(0, 0, 0));
        return 1;
    }
    if (strcmp(args, "stat") == 0) {
        char st[128] = {0};
        kprintf("stat(.) -> %d\n", sys_stat(".", st));
        return 1;
    }
    if (strcmp(args, "exec") == 0) {
        kprintf("exec(.) -> %d\n", sys_exec("."));
        return 1;
    }

    kprintf("Unknown syscall command: %s\n", args);
    return 0;
}
