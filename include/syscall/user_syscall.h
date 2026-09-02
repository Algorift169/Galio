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
static inline int sys_sleep(unsigned int milliseconds) { return (int)galio_syscall(SYS_SLEEP, milliseconds, 0, 0, 0, 0); }
static inline int sys_sched_yield(void) { return (int)galio_syscall(SYS_SCHED_YIELD, 0, 0, 0, 0, 0); }
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
static inline int sys_socket(int domain, int type, int protocol) { return (int)galio_syscall(SYS_SOCKET, domain, type, protocol, 0, 0); }
static inline int sys_bind(int sockfd, const void *addr, unsigned int addrlen) { return (int)galio_syscall(SYS_BIND, sockfd, (long)addr, addrlen, 0, 0); }
static inline int sys_listen(int sockfd, int backlog) { return (int)galio_syscall(SYS_LISTEN, sockfd, backlog, 0, 0, 0); }
static inline int sys_accept(int sockfd, void *addr, unsigned int *addrlen) { return (int)galio_syscall(SYS_ACCEPT, sockfd, (long)addr, (long)addrlen, 0, 0); }
static inline int sys_connect(int sockfd, const void *addr, unsigned int addrlen) { return (int)galio_syscall(SYS_CONNECT, sockfd, (long)addr, addrlen, 0, 0); }
static inline int sys_semget(unsigned int key, int nsems, int semflg) { return (int)galio_syscall(SYS_SEMGET, key, nsems, semflg, 0, 0); }
static inline int sys_semop(int semid, void *sops, unsigned int nsops) { return (int)galio_syscall(SYS_SEMOP, semid, (long)sops, nsops, 0, 0); }
static inline int sys_semctl(int semid, int semnum, int cmd, void *arg) { return (int)galio_syscall(SYS_SEMCTL, semid, semnum, cmd, (long)arg, 0); }
static inline void *sys_shmat(int shmid, const void *shmaddr, int shmflg) { return (void *)galio_syscall(SYS_SHMAT, shmid, (long)shmaddr, shmflg, 0, 0); }
static inline int sys_shmdt(const void *shmaddr) { return (int)galio_syscall(SYS_SHMDT, (long)shmaddr, 0, 0, 0, 0); }
static inline int sys_shmget(unsigned int key, unsigned int size, int shmflg) { return (int)galio_syscall(SYS_SHMGET, key, size, shmflg, 0, 0); }
static inline int sys_shmctl(int shmid, int cmd, void *buf) { return (int)galio_syscall(SYS_SHMCTL, shmid, cmd, (long)buf, 0, 0); }

#endif /* USER_SYSCALL_H */
