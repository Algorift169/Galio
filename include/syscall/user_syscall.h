#ifndef USER_SYSCALL_H
#define USER_SYSCALL_H

#include "syscall.h"

static inline long galio_syscall(long nr, long a1, long a2, long a3, long a4, long a5) {
    long ret;
    __asm__ volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(nr), "b"(a1), "c"(a2), "d"(a3), "S"(a4), "D"(a5)
        : "memory"
    );
    return ret;
}

static inline int sys_exit(int status) { return (int)galio_syscall(SYS_EXIT, status, 0, 0, 0, 0); }
static inline int sys_write(int fd, const void *buf, unsigned long size) { return (int)galio_syscall(SYS_WRITE, fd, (long)buf, (long)size, 0, 0); }
static inline int sys_getpid(void) { return (int)galio_syscall(SYS_GETPID, 0, 0, 0, 0, 0); }
static inline int sys_fork(void) { return (int)galio_syscall(SYS_FORK, 0, 0, 0, 0, 0); }
static inline int sys_exec(const char *path) { return (int)galio_syscall(SYS_EXEC, (long)path, 0, 0, 0, 0); }
static inline int sys_execve(const char *path, char *const argv[], char *const envp[]) { return (int)galio_syscall(SYS_EXECVE, (long)path, (long)argv, (long)envp, 0, 0); }
static inline int sys_waitpid(int pid) { return (int)galio_syscall(SYS_WAITPID, pid, 0, 0, 0, 0); }
static inline int sys_open(const char *path, int flags) { return (int)galio_syscall(SYS_OPEN, (long)path, flags, 0, 0, 0); }
static inline int sys_read(int fd, void *buffer, unsigned long size) { return (int)galio_syscall(SYS_READ, fd, (long)buffer, (long)size, 0, 0); }
static inline int sys_close(int fd) { return (int)galio_syscall(SYS_CLOSE, fd, 0, 0, 0, 0); }
static inline long sys_lseek(int fd, long offset, int whence) { return galio_syscall(SYS_LSEEK, fd, offset, whence, 0, 0); }
static inline int sys_stat(const char *path, void *statbuf) { return (int)galio_syscall(SYS_STAT, (long)path, (long)statbuf, 0, 0, 0); }
static inline void *sys_mmap(void *addr, unsigned long length, int prot, int flags, int fd, unsigned long offset) { return (void *)galio_syscall(SYS_MMAP, (long)addr, (long)length, prot, flags, (long)fd); }
static inline int sys_munmap(void *addr, unsigned long length) { return (int)galio_syscall(SYS_MUNMAP, (long)addr, (long)length, 0, 0, 0); }
static inline void *sys_brk(void *addr) { return (void *)galio_syscall(SYS_BRK, (long)addr, 0, 0, 0, 0); }
static inline int sys_pipe(int pipefd[2]) { return (int)galio_syscall(SYS_PIPE, (long)pipefd, 0, 0, 0, 0); }
static inline int sys_dup(int oldfd) { return (int)galio_syscall(SYS_DUP, oldfd, 0, 0, 0, 0); }
static inline int sys_dup2(int oldfd, int newfd) { return (int)galio_syscall(SYS_DUP2, oldfd, newfd, 0, 0, 0); }
static inline int sys_chdir(const char *path) { return (int)galio_syscall(SYS_CHDIR, (long)path, 0, 0, 0, 0); }
static inline int sys_getcwd(char *buffer, unsigned long size) { return (int)galio_syscall(SYS_GETCWD, (long)buffer, (long)size, 0, 0, 0); }
static inline long sys_time(void) { return galio_syscall(SYS_TIME, 0, 0, 0, 0, 0); }
static inline int sys_gettimeofday(void *tv) { return (int)galio_syscall(SYS_GETTIMEOFDAY, (long)tv, 0, 0, 0, 0); }
static inline int sys_getuid(void) { return (int)galio_syscall(SYS_GETUID, 0, 0, 0, 0, 0); }
static inline int sys_getgid(void) { return (int)galio_syscall(SYS_GETGID, 0, 0, 0, 0, 0); }

#endif /* USER_SYSCALL_H */
