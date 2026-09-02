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

/* syscall.c - System calls interface */
#include "syscall.h"
#include "tss.h"
#include "cpu.h"
#include "vga.h"
#include "kprintf.h"
#include "process.h"
#include "vfs.h"
#include "elf.h"
#include "heap.h"
#include "paging.h"
#include "pmem.h"
#include "string.h"

/* Forward declarations for syscall implementations */
static u32 syscall_fork(void);
static i32 syscall_exec(const char *path);
static i32 syscall_waitpid(i32 pid);
static u32 syscall_open(const char *path, i32 flags);
static u32 syscall_write(u32 fd, const void *buffer, u32 size);
static u32 syscall_read(u32 fd, void *buffer, u32 size);
static u32 syscall_close(u32 fd);
static u32 syscall_lseek(u32 fd, i32 offset, i32 whence);
static u32 syscall_stat(const char *path, void *statbuf);
static void *syscall_mmap(void *addr, u32 length, i32 prot, i32 flags, i32 fd, u32 offset);
static i32 syscall_munmap(void *addr, u32 length);
static void *syscall_brk(void *addr);

/* Extended syscall declarations from syscall_extra.c */
extern i32 syscall_lstat(const char *path, struct stat *statbuf);
extern i32 syscall_mprotect(void *addr, u32 len, i32 prot);
extern i64 syscall_pread64(u32 fd, void *buf, u32 count, u64 offset);
extern i64 syscall_pwrite64(u32 fd, const void *buf, u32 count, u64 offset);
extern i32 syscall_readv(u32 fd, void *iov, i32 iovcnt);
extern i32 syscall_writev(u32 fd, const void *iov, i32 iovcnt);
extern i32 syscall_select(i32 nfds, void *readfds, void *writefds, void *exceptfds, struct timespec *timeout);
extern i32 syscall_sched_yield(void);
extern i32 syscall_pause(void);
extern i32 syscall_socket(i32 domain, i32 type, i32 protocol);
extern i32 syscall_connect(i32 sockfd, const void *addr, u32 addrlen);
extern i32 syscall_accept(i32 sockfd, void *addr, u32 *addrlen);
extern i32 syscall_sendto(i32 sockfd, const void *buf, u32 len, i32 flags, const void *dest_addr, u32 addrlen);
extern i32 syscall_recvfrom(i32 sockfd, void *buf, u32 len, i32 flags, void *src_addr, u32 *addrlen);
extern i32 syscall_sendmsg(i32 sockfd, const void *msg, i32 flags);
extern i32 syscall_recvmsg(i32 sockfd, void *msg, i32 flags);
extern i32 syscall_shutdown(i32 sockfd, i32 how);
extern i32 syscall_bind(i32 sockfd, const void *addr, u32 addrlen);
extern i32 syscall_listen(i32 sockfd, i32 backlog);
extern i32 syscall_getsockname(i32 sockfd, void *addr, u32 *addrlen);
extern i32 syscall_getpeername(i32 sockfd, void *addr, u32 *addrlen);
extern i32 syscall_socketpair(i32 domain, i32 type, i32 protocol, i32 *sv);
extern i32 syscall_setsockopt(i32 sockfd, i32 level, i32 optname, const void *optval, u32 optlen);
extern i32 syscall_getsockopt(i32 sockfd, i32 level, i32 optname, void *optval, u32 *optlen);
extern u32 syscall_clone(u32 flags, void *stack, i32 *ptid, void *tls, i32 *ctid);
extern u32 syscall_vfork(void);
extern i32 syscall_wait4(i32 pid, i32 *wstatus, i32 options, void *rusage);
extern i32 syscall_semget(u32 key, i32 nsems, i32 semflg);
extern i32 syscall_semop(i32 semid, void *sops, u32 nsops);
extern i32 syscall_semctl(i32 semid, i32 semnum, i32 cmd, void *arg);
extern void *syscall_shmat(i32 shmid, const void *shmaddr, i32 shmflg);
extern i32 syscall_shmdt(const void *shmaddr);
extern i32 syscall_shmget(u32 key, u32 size, i32 shmflg);
extern i32 syscall_shmctl(i32 shmid, i32 cmd, void *buf);
extern i32 syscall_rt_sigaction(i32 sig, const void *act, void *oldact, u32 sigsetsize);
extern i32 syscall_rt_sigprocmask(i32 how, const void *set, void *oldset, u32 sigsetsize);
extern i32 syscall_rt_sigreturn(void);
extern i32 syscall_sysinfo(void *info);

#define USER_STRING_MAX PROCESS_PATH_MAX
#define SIMPLE_STAT_SIZE (sizeof(u32) * 6)

u8 validate_user_buffer(const void *ptr, u32 length, u8 write) {
    if (length == 0) return 1;
    if (!ptr) return 0; /* reject NULL unless length == 0 */

    uintptr_t start = (uintptr_t)ptr;
    uintptr_t end = start + length - 1;
    /* Detect wraparound/overflow */
    if (end < start) return 0;
    /* Reject any range that touches kernel space */
    if (end >= KERNEL_SPACE_START) return 0;

    process_t *proc = process_current();
    if (!proc || !proc->pagedir) return 0;
    return paging_validate_user_range((page_directory_t *)proc->pagedir, start, length, write);
}

u8 validate_user_ptr(const void *ptr, u8 write) {
    return validate_user_buffer(ptr, 1, write);
}

u8 validate_user_string(const char *str, u32 max_length) {
    if (max_length == 0) return 0;
    if (!str) return 0;

    for (u32 i = 0; i < max_length; i++) {
        if (!validate_user_ptr(str + i, 0)) {
            return 0;
        }
        if (str[i] == 0) {
            return 1;
        }
    }

    return 0;
}

static u8 copy_user_string(char *dst, const char *src, u32 dst_size) {
    if (!dst || dst_size == 0 || !validate_user_string(src, dst_size)) {
        return 0;
    }
    for (u32 i = 0; i < dst_size; i++) {
        dst[i] = src[i];
        if (dst[i] == 0) {
            return 1;
        }
    }
    dst[dst_size - 1] = 0;
    return 0;
}

/*
 * Syscall ABI and usage
 * ---------------------
 *
 * User code invokes a syscall with INT 0x80.  The assembly entry preserves
 * the registers and calls this function with a pointer to that register
 * frame.  The dispatcher reads and writes the frame, so the value assigned to
 * regs->rax is returned to the caller when the interrupt returns.
 *
 * Register convention:
 *   rax = syscall number
 *   rbx = argument 1
 *   rcx = argument 2
 *   rdx = argument 3
 *   rsi = argument 4
 *   rdi = argument 5
 *
 * Negative return values report failure.  This kernel does not maintain a
 * separate errno value.  Pointer arguments must refer to mapped user memory;
 * pointer-taking implementations validate them before reading or writing.
 *
 * Supported syscall usage:
 *   SYS_READ(fd, buffer, count)       Read from a process FD or pipe.
 *   SYS_WRITE(fd, buffer, count)      Write to a process FD, pipe, or stdout.
 *   SYS_OPEN(path, flags)             Open a VFS path and return a process FD.
 *   SYS_CLOSE(fd)                     Close a process FD.
 *   SYS_LSEEK(fd, offset, whence)     Change the position of an open VFS file.
 *   SYS_STAT(path, statbuf)            Get VFS metadata using the legacy layout.
 *   SYS_MMAP(addr, len, prot, flags)   Map anonymous user pages.
 *   SYS_MUNMAP(addr, len)              Release pages in the user heap range.
 *   SYS_BRK(addr)                     Read or grow the process heap break.
 *   SYS_SCHED_YIELD()                 Yield to the process scheduler.
 *   SYS_GETPID()                      Return the current process ID.
 *   SYS_FORK()                        Clone process state and its page tables.
 *   SYS_EXEC(path)                    Load an ELF image from the VFS.
 *   SYS_EXECVE(path, argv, envp)      Load an ELF image with argument vectors.
 *   SYS_EXIT(status)                  Terminate the current process.
 *   SYS_WAITPID(pid) / SYS_WAIT(pid) Wait for an existing child process.
 *   SYS_WAIT4(pid, status, options, rusage)
 *                                     Use the currently supported wait path.
 *   SYS_KILL(pid, signal)             Deliver a signal through process_kill.
 *   SYS_GETPPID()                     Return the current parent process ID.
 *   SYS_GETUID()/SYS_GETGID()         Return process credentials.
 *   SYS_GETEUID()/SYS_GETEGID()       Return effective process credentials.
 *   SYS_CHDIR(path)                   Change the process working directory.
 *   SYS_GETCWD(buffer, size)          Copy the working directory to user space.
 *   SYS_TIME()                        Return wall-clock seconds since the epoch.
 *   SYS_GETTIMEOFDAY(tv, tz)          Return wall-clock seconds and microseconds.
 *   SYS_CLOCK_GETTIME(clock, tp)      Return the kernel wall-clock timespec.
 *   SYS_NANOSLEEP(req, rem)           Delay using the kernel time source.
 *   SYS_SLEEP(milliseconds)           Delay using the PIT-backed timer helper.
 *   SYS_UNAME(buffer)                 Copy Galio and architecture identity data.
 *   SYS_ACCESS(path, mode)            Check that a VFS path exists.
 *   SYS_READLINK(path, buffer, size)  Read a VFS symbolic link.
 *   SYS_PIPE(pipefd[2])               Create a process pipe pair.
 *   SYS_DUP(oldfd) / SYS_DUP2(old,new)
 *                                     Duplicate process FD entries.
 *
 * Reserved but unsupported calls (socket, semaphore, shared memory, signal
 * disposition, poll/ioctl, vector I/O, positioned I/O, mprotect, lstat,
 * clone, and vfork) return an ENOSYS-style negative result from their real
 * implementation.  They are dispatched only to preserve stable syscall
 * numbers; they must not be documented or treated as working capabilities.
 */
static void syscall_handler(registers_t *regs) {
    /* For INT 0x80, we need to distinguish syscall number from interrupt_number */
    /* The actual syscall number is in EAX */
    u64 syscall_num = regs->rax;
    u64 arg1 = regs->rbx;
    u64 arg2 = regs->rcx;
    u64 arg3 = regs->rdx;
    u64 arg4 = regs->rsi;
    u64 arg5 = regs->rdi;
    u64 arg6 = 0;  /* Would need to come from stack if needed */

#if SYSCALL_TRACE
    const char *name = "unknown";
    /*
     * Each case below is the kernel side of the corresponding user wrapper.
     * Scalar arguments are copied from the saved register frame.  Pointer
     * arguments are passed to a syscall implementation, which owns the
     * required user-range validation and VFS/process permission checks.
     */
    switch (syscall_num) {
        case SYS_EXIT: name = "exit"; break;
        case SYS_WRITE: name = "write"; break;
        case SYS_GETPID: name = "getpid"; break;
        case SYS_FORK: name = "fork"; break;
        case SYS_EXEC: name = "exec"; break;
        case SYS_WAITPID: name = "waitpid"; break;
        case SYS_OPEN: name = "open"; break;
        case SYS_READ: name = "read"; break;
        case SYS_CLOSE: name = "close"; break;
        case SYS_LSEEK: name = "lseek"; break;
        case SYS_STAT: name = "stat"; break;
        case SYS_FSTAT: name = "fstat"; break;
        case SYS_LSTAT: name = "lstat"; break;
        case SYS_MMAP: name = "mmap"; break;
        case SYS_MUNMAP: name = "munmap"; break;
        case SYS_BRK: name = "brk"; break;
        case SYS_MPROTECT: name = "mprotect"; break;
        case SYS_EXECVE: name = "execve"; break;
        case SYS_PIPE: name = "pipe"; break;
        case SYS_DUP: name = "dup"; break;
        case SYS_DUP2: name = "dup2"; break;
        case SYS_CHDIR: name = "chdir"; break;
        case SYS_GETCWD: name = "getcwd"; break;
        case SYS_TIME: name = "time"; break;
        case SYS_GETTIMEOFDAY: name = "gettimeofday"; break;
        case SYS_GETUID: name = "getuid"; break;
        case SYS_GETGID: name = "getgid"; break;
        case SYS_GETEUID: name = "geteuid"; break;
        case SYS_GETEGID: name = "getegid"; break;
        case SYS_GETPPID: name = "getppid"; break;
        case SYS_KILL: name = "kill"; break;
        case SYS_UNAME: name = "uname"; break;
        case SYS_READLINK: name = "readlink"; break;
        case SYS_ACCESS: name = "access"; break;
        case SYS_CLOCK_GETTIME: name = "clock_gettime"; break;
        case SYS_NANOSLEEP: name = "nanosleep"; break;
        case SYS_IOCTL: name = "ioctl"; break;
        case SYS_POLL: name = "poll"; break;
        case SYS_PREAD64: name = "pread64"; break;
        case SYS_PWRITE64: name = "pwrite64"; break;
        case SYS_READV: name = "readv"; break;
        case SYS_WRITEV: name = "writev"; break;
        case SYS_SELECT: name = "select"; break;
        case SYS_SCHED_YIELD: name = "sched_yield"; break;
        case SYS_PAUSE: name = "pause"; break;
        case SYS_SOCKET: name = "socket"; break;
        case SYS_CONNECT: name = "connect"; break;
        case SYS_ACCEPT: name = "accept"; break;
        case SYS_SENDTO: name = "sendto"; break;
        case SYS_RECVFROM: name = "recvfrom"; break;
        case SYS_SENDMSG: name = "sendmsg"; break;
        case SYS_RECVMSG: name = "recvmsg"; break;
        case SYS_SHUTDOWN: name = "shutdown"; break;
        case SYS_BIND: name = "bind"; break;
        case SYS_LISTEN: name = "listen"; break;
        case SYS_GETSOCKNAME: name = "getsockname"; break;
        case SYS_GETPEERNAME: name = "getpeername"; break;
        case SYS_SOCKETPAIR: name = "socketpair"; break;
        case SYS_SETSOCKOPT: name = "setsockopt"; break;
        case SYS_GETSOCKOPT: name = "getsockopt"; break;
        case SYS_CLONE: name = "clone"; break;
        case SYS_VFORK: name = "vfork"; break;
        case SYS_WAIT4: name = "wait4"; break;
        case SYS_WAIT: name = "wait"; break;
        case SYS_SEMGET: name = "semget"; break;
        case SYS_SEMOP: name = "semop"; break;
        case SYS_SEMCTL: name = "semctl"; break;
        case SYS_SHMDT: name = "shmdt"; break;
        case SYS_SHMGET: name = "shmget"; break;
        case SYS_SHMAT: name = "shmat"; break;
        case SYS_SHMCTL: name = "shmctl"; break;
        case SYS_RT_SIGACTION: name = "rt_sigaction"; break;
        case SYS_RT_SIGPROCMASK: name = "rt_sigprocmask"; break;
        case SYS_RT_SIGRETURN: name = "rt_sigreturn"; break;
        case SYS_SYSINFO: name = "sysinfo"; break;
        case SYS_MMAP2: name = "mmap2"; break;
    }
    kprintf("[SYSCALL] pid=%u nr=%llu %s()\n", process_current() ? process_current()->pid : 0, (unsigned long long)syscall_num, name);
#endif

    switch (syscall_num) {
        case SYS_EXIT:
            kprintf("Process exit with code %d\n", (i32)arg1);
            process_exit((i32)arg1);
            break;
            
        case SYS_WRITE: {
            if (arg1 == 1) {  /* stdout */
                if (!validate_user_buffer((const void *)arg2, (u32)arg3, 0)) {
                    regs->rax = (u64)-1;
                    break;
                }
                for (u32 i = 0; i < arg3; i++) {
                    vga_putch(((char *)arg2)[i]);
                }
                regs->rax = arg3;
            } else {
                regs->rax = syscall_write((u32)arg1, (const void *)arg2, (u32)arg3);
            }
            break;
        }

        case SYS_GETPID: {
            process_t *proc = process_current();
            regs->rax = proc ? proc->pid : 0;
            break;
        }

        case SYS_FORK:
            regs->rax = syscall_fork();
            break;

        case SYS_EXEC:
            regs->rax = syscall_exec((const char *)arg1);
            break;

        case SYS_EXECVE:
            regs->rax = syscall_execve((const char *)arg1, (char *const *)arg2, (char *const *)arg3);
            break;

        case SYS_SLEEP:
            syscall_sleep((u32)arg1);
            regs->rax = 0;
            break;

        case SYS_WAITPID:
            regs->rax = syscall_waitpid((i32)arg1);
            break;

        case SYS_PIPE:
            regs->rax = syscall_pipe((i32 *)arg1);
            break;

        case SYS_DUP:
            regs->rax = syscall_dup((u32)arg1);
            break;

        case SYS_DUP2:
            regs->rax = syscall_dup2((u32)arg1, (u32)arg2);
            break;

        case SYS_CHDIR:
            regs->rax = syscall_chdir((const char *)arg1);
            break;

        case SYS_GETCWD:
            regs->rax = syscall_getcwd((char *)arg1, (u32)arg2);
            break;

        case SYS_TIME:
            regs->rax = syscall_time();
            break;

        case SYS_GETTIMEOFDAY:
            regs->rax = syscall_gettimeofday((struct timeval *)arg1, (void *)arg2);
            break;

        case SYS_GETUID:
            regs->rax = syscall_getuid();
            break;

        case SYS_GETGID:
            regs->rax = syscall_getgid();
            break;

        case SYS_GETEUID:
            regs->rax = syscall_geteuid();
            break;

        case SYS_GETEGID:
            regs->rax = syscall_getegid();
            break;

        case SYS_GETPPID:
            regs->rax = syscall_getppid();
            break;

        case SYS_KILL:
            regs->rax = syscall_kill((u32)arg1, (i32)arg2);
            break;

        case SYS_UNAME:
            regs->rax = syscall_uname((struct utsname *)arg1);
            break;

        case SYS_ACCESS:
            regs->rax = syscall_access((const char *)arg1, (i32)arg2);
            break;

        case SYS_FSTAT:
            regs->rax = syscall_fstat((int)arg1, (struct stat *)arg2);
            break;

        case SYS_READLINK:
            regs->rax = syscall_readlink((const char *)arg1, (char *)arg2, (u32)arg3);
            break;

        case SYS_CLOCK_GETTIME:
            regs->rax = syscall_clock_gettime((i32)arg1, (void *)arg2);
            break;

        case SYS_NANOSLEEP:
            regs->rax = syscall_nanosleep((const struct timespec *)arg1, (struct timespec *)arg2);
            break;

        case SYS_IOCTL:
            regs->rax = syscall_ioctl((u32)arg1, (u32)arg2, (void *)arg3);
            break;

        case SYS_POLL:
            regs->rax = syscall_poll((void *)arg1, (u32)arg2, (i32)arg3);
            break;

        case SYS_OPEN:
            regs->rax = syscall_open((const char *)arg1, (i32)arg2);
            break;

        case SYS_READ:
            regs->rax = syscall_read((u32)arg1, (void *)arg2, (u32)arg3);
            break;

        case SYS_CLOSE:
            regs->rax = syscall_close((u32)arg1);
            break;

        case SYS_LSEEK:
            regs->rax = syscall_lseek((u32)arg1, (i32)arg2, (i32)arg3);
            break;

        case SYS_STAT:
            regs->rax = syscall_stat((const char *)arg1, (void *)arg2);
            break;

        case SYS_MMAP:
            regs->rax = (uintptr_t)syscall_mmap((void *)arg1, (u32)arg2, (i32)arg3, (i32)arg4, (i32)arg5, (u32)arg6);
            break;

        case SYS_MUNMAP:
            regs->rax = syscall_munmap((void *)arg1, (u32)arg2);
            break;

        case SYS_BRK:
            regs->rax = (uintptr_t)syscall_brk((void *)arg1);
            break;

        case SYS_LSTAT:
            regs->rax = syscall_lstat((const char *)arg1, (struct stat *)arg2);
            break;

        case SYS_MPROTECT:
            regs->rax = syscall_mprotect((void *)arg1, (u32)arg2, (i32)arg3);
            break;

        case SYS_PREAD64:
            regs->rax = syscall_pread64((u32)arg1, (void *)arg2, (u32)arg3, (u64)arg4);
            break;

        case SYS_PWRITE64:
            regs->rax = syscall_pwrite64((u32)arg1, (const void *)arg2, (u32)arg3, (u64)arg4);
            break;

        case SYS_READV:
            regs->rax = syscall_readv((u32)arg1, (void *)arg2, (i32)arg3);
            break;

        case SYS_WRITEV:
            regs->rax = syscall_writev((u32)arg1, (const void *)arg2, (i32)arg3);
            break;

        case SYS_SELECT:
            regs->rax = syscall_select((i32)arg1, (void *)arg2, (void *)arg3, (void *)arg4, (struct timespec *)arg5);
            break;

        case SYS_SCHED_YIELD:
            regs->rax = syscall_sched_yield();
            break;

        case SYS_PAUSE:
            regs->rax = syscall_pause();
            break;

        case SYS_SOCKET:
            regs->rax = syscall_socket((i32)arg1, (i32)arg2, (i32)arg3);
            break;

        case SYS_CONNECT:
            regs->rax = syscall_connect((i32)arg1, (const void *)arg2, (u32)arg3);
            break;

        case SYS_ACCEPT:
            regs->rax = syscall_accept((i32)arg1, (void *)arg2, (u32 *)arg3);
            break;

        case SYS_SENDTO:
            regs->rax = syscall_sendto((i32)arg1, (const void *)arg2, (u32)arg3, (i32)arg4, (const void *)arg5, (u32)arg6);
            break;

        case SYS_RECVFROM:
            regs->rax = syscall_recvfrom((i32)arg1, (void *)arg2, (u32)arg3, (i32)arg4, (void *)arg5, (u32 *)arg6);
            break;

        case SYS_SENDMSG:
            regs->rax = syscall_sendmsg((i32)arg1, (const void *)arg2, (i32)arg3);
            break;

        case SYS_RECVMSG:
            regs->rax = syscall_recvmsg((i32)arg1, (void *)arg2, (i32)arg3);
            break;

        case SYS_SHUTDOWN:
            regs->rax = syscall_shutdown((i32)arg1, (i32)arg2);
            break;

        case SYS_BIND:
            regs->rax = syscall_bind((i32)arg1, (const void *)arg2, (u32)arg3);
            break;

        case SYS_LISTEN:
            regs->rax = syscall_listen((i32)arg1, (i32)arg2);
            break;

        case SYS_GETSOCKNAME:
            regs->rax = syscall_getsockname((i32)arg1, (void *)arg2, (u32 *)arg3);
            break;

        case SYS_GETPEERNAME:
            regs->rax = syscall_getpeername((i32)arg1, (void *)arg2, (u32 *)arg3);
            break;

        case SYS_SOCKETPAIR:
            regs->rax = syscall_socketpair((i32)arg1, (i32)arg2, (i32)arg3, (i32 *)arg4);
            break;

        case SYS_SETSOCKOPT:
            regs->rax = syscall_setsockopt((i32)arg1, (i32)arg2, (i32)arg3, (const void *)arg4, (u32)arg5);
            break;

        case SYS_GETSOCKOPT:
            regs->rax = syscall_getsockopt((i32)arg1, (i32)arg2, (i32)arg3, (void *)arg4, (u32 *)arg5);
            break;

        case SYS_CLONE:
            regs->rax = syscall_clone((u32)arg1, (void *)arg2, (i32 *)arg3, (void *)arg4, (i32 *)arg5);
            break;

        case SYS_VFORK:
            regs->rax = syscall_vfork();
            break;

        case SYS_WAIT4:
            regs->rax = syscall_wait4((i32)arg1, (i32 *)arg2, (i32)arg3, (void *)arg4);
            break;

        case SYS_WAIT:
            regs->rax = syscall_waitpid((i32)arg1);
            break;

        case SYS_SEMGET:
            regs->rax = syscall_semget((u32)arg1, (i32)arg2, (i32)arg3);
            break;

        case SYS_SEMOP:
            regs->rax = syscall_semop((i32)arg1, (void *)arg2, (u32)arg3);
            break;

        case SYS_SEMCTL:
            regs->rax = syscall_semctl((i32)arg1, (i32)arg2, (i32)arg3, (void *)arg4);
            break;

        case SYS_SHMDT:
            regs->rax = syscall_shmdt((const void *)arg1);
            break;

        case SYS_SHMGET:
            regs->rax = syscall_shmget((u32)arg1, (u32)arg2, (i32)arg3);
            break;

        case SYS_SHMAT:
            regs->rax = (uintptr_t)syscall_shmat((i32)arg1, (const void *)arg2, (i32)arg3);
            break;

        case SYS_SHMCTL:
            regs->rax = syscall_shmctl((i32)arg1, (i32)arg2, (void *)arg3);
            break;

        case SYS_RT_SIGACTION:
            regs->rax = syscall_rt_sigaction((i32)arg1, (const void *)arg2, (void *)arg3, (u32)arg4);
            break;

        case SYS_RT_SIGPROCMASK:
            regs->rax = syscall_rt_sigprocmask((i32)arg1, (const void *)arg2, (void *)arg3, (u32)arg4);
            break;

        case SYS_RT_SIGRETURN:
            regs->rax = syscall_rt_sigreturn();
            break;

        case SYS_SYSINFO:
            regs->rax = syscall_sysinfo((void *)arg1);
            break;

        case SYS_MMAP2:
            regs->rax = (uintptr_t)syscall_mmap((void *)arg1, (u32)arg2, (i32)arg3, (i32)arg4, (i32)arg5, (u32)arg6);
            break;

        default:
            kprintf("Unknown syscall: %u\n", (u32)syscall_num);
            regs->rax = (u64)-1;
            break;
    }
}

/* Syscall implementations */

static u32 syscall_fork(void) {
    process_t *current = process_current();
    if (!current) return -1;

    /* Create child process */
    u32 child_pid = process_create(NULL, current->priority);
    if (!child_pid) return -1;

    process_t *child = process_get(child_pid);
    if (!child) return -1;

    process_set_path(child, current->path);

    /* Copy parent's register state */
    memcpy(&child->regs, &current->regs, sizeof(register_state_t));

    /* Child gets 0 return value */
    child->regs.eax = 0;

    /* Replace child's default address space with a clone of the parent.
     * Free the child's original address space first to avoid leaking the
     * page directory created by process_create(). If cloning fails, clean
     * up the child process to avoid leaking resources.
     */
    process_free_address_space(child);
    page_directory_t *clone = paging_clone_directory(current->pagedir);
    if (!clone) {
        /* cloning failed, clean up child and return error */
        child->state = PROCESS_ZOMBIE;
        process_reap(child);
        return -1;
    }
    child->pagedir = clone;

    kprintf("Fork: parent PID=%u, child PID=%u\n", current->pid, child_pid);
    return child_pid;
}

static u32 syscall_write(u32 fd, const void *buffer, u32 size) {
    process_t *proc = process_current();
    if (!proc) {
        return (u32)-1;
    }
    if (fd >= PROCESS_MAX_FDS) {
        return (u32)-1;
    }
    if (!validate_user_buffer(buffer, size, 0)) {
        return (u32)-1;
    }

    u32 handle = proc->fd_table[fd];
    if (handle == VFS_INVALID_FD) {
        return (u32)-1;
    }

    if (handle & PIPE_FD_FLAG) {
        return pipe_write_fd(handle, buffer, size);
    }

    return vfs_write(handle, buffer, size);
}

static i32 syscall_exec(const char *path) {
    process_t *proc = process_current();
    if (!proc || !path) {
        return -1;
    }

    char kpath[PROCESS_PATH_MAX];
    if (!copy_user_string(kpath, path, sizeof(kpath))) {
        return -1;
    }

    char resolved[PROCESS_PATH_MAX];
    if (!process_resolve_path(proc->cwd, kpath, resolved, PROCESS_PATH_MAX)) {
        return -1;
    }

    /* Load ELF from VFS */
    void *elf_data = kmalloc(65536);  /* Temporary buffer */
    if (!elf_data) return -1;

    u32 size = vfs_read(resolved, elf_data, 65536);
    if (!size) {
        kfree(elf_data);
        return -1;
    }

    /* Load ELF into the process page directory */
    process_t *current = process_current();
    page_directory_t *saved_pd = paging_get_current();
    if (current->pagedir) {
        paging_load_directory(current->pagedir);
    }

    u32 entry = elf_load(elf_data, size);
    kfree(elf_data);

    if (!entry) {
        if (current->pagedir && saved_pd && saved_pd != current->pagedir) {
            paging_load_directory(saved_pd);
        }
        return -1;
    }

    process_set_path(current, resolved);

    /* Switch to user mode */
    current->regs.eip = entry;
    current->regs.esp = USER_STACK_TOP;  /* Kernel stack top for the process */
    current->regs.user_esp = USER_STACK_TOP;
    current->regs.user_ss = USER_DS;
    current->regs.cs = USER_CS;
    current->regs.eflags &= ~0x3000;    /* Clear IOPL for ring3 */
    current->regs.eflags |= 0x202;      /* Set IF and reserved flag */

    if (current->pagedir) {
        paging_load_directory(current->pagedir);
    }
    tss_set_kernel_stack((uintptr_t)current->stack + current->stack_size - 8);

    /* Set up user-mode segments (assuming GDT has user descriptors) */
    __asm__ volatile(
        "mov $0x23, %%rax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "pushq $0x23\n"
        "pushq %0\n"
        "pushfq\n"
        "orq $0x200, (%%rsp)\n"
        "pushq $0x1B\n"
        "pushq %1\n"
        "iretq\n"
        : : "r"(current->regs.user_esp), "r"((uintptr_t)entry)
        : "rax", "memory"
    );

    /* Should not reach here */
    return -1;
}

static i32 syscall_waitpid(i32 pid) {
    return process_waitpid(pid);
}

static u32 syscall_open(const char *path, i32 flags) {
    (void)flags;
    
    process_t *proc = process_current();
    if (!proc || !path) return -1;

    char kpath[PROCESS_PATH_MAX];
    if (!copy_user_string(kpath, path, sizeof(kpath))) {
        return (u32)-1;
    }

    char resolved[PROCESS_PATH_MAX];
    if (!process_resolve_path(proc->cwd, kpath, resolved, PROCESS_PATH_MAX)) {
        return -1;
    }
    
    /* Open file in VFS */
    u32 vfs_fd = vfs_open(resolved);
    if (vfs_fd == VFS_INVALID_FD) return -1;
    
    /* Find a free entry in process FD table */
    for (u32 i = 0; i < PROCESS_MAX_FDS; i++) {
        if (proc->fd_table[i] == VFS_INVALID_FD) {
            proc->fd_table[i] = vfs_fd;
            return i;  /* Return process-relative FD */
        }
    }
    
    /* No free FD slots in process table, close and return error */
    vfs_close(vfs_fd);
    return -1;
}

static u32 syscall_read(u32 fd, void *buffer, u32 size) {
    process_t *proc = process_current();
    if (!proc) return -1;
    if (fd >= PROCESS_MAX_FDS) return -1;
    if (!validate_user_buffer(buffer, size, 1)) return (u32)-1;
    
    u32 handle = proc->fd_table[fd];
    if (handle == VFS_INVALID_FD) return -1;

    if (handle & PIPE_FD_FLAG) {
        return pipe_read_fd(handle, buffer, size);
    }

    return vfs_read_fd(handle, buffer, size);
}

static u32 syscall_close(u32 fd) {
    process_t *proc = process_current();
    if (!proc) return -1;
    if (fd >= PROCESS_MAX_FDS) return -1;
    
    u32 handle = proc->fd_table[fd];
    if (handle == VFS_INVALID_FD) return -1;
    
    u32 result;
    if (handle & PIPE_FD_FLAG) {
        result = pipe_close_handle(handle);
    } else {
        result = vfs_close(handle);
    }
    
    proc->fd_table[fd] = VFS_INVALID_FD;
    
    return result;
}

static u32 syscall_lseek(u32 fd, i32 offset, i32 whence) {
    process_t *proc = process_current();
    if (!proc) return (u32)-1;
    if (fd >= PROCESS_MAX_FDS) return (u32)-1;
    
    u32 vfs_fd = proc->fd_table[fd];
    if (vfs_fd == VFS_INVALID_FD) return (u32)-1;
    
    return vfs_lseek(vfs_fd, offset, whence);
}

static u32 syscall_stat(const char *path, void *statbuf) {
    if (!path || !statbuf) return -1;
    if (!validate_user_buffer(statbuf, SIMPLE_STAT_SIZE, 1)) return (u32)-1;

    process_t *proc = process_current();
    if (!proc) return -1;

    char kpath[PROCESS_PATH_MAX];
    if (!copy_user_string(kpath, path, sizeof(kpath))) {
        return (u32)-1;
    }

    char resolved[PROCESS_PATH_MAX];
    if (!process_resolve_path(proc->cwd, kpath, resolved, PROCESS_PATH_MAX)) {
        return -1;
    }

    return vfs_stat(resolved, statbuf);
}

static void *syscall_mmap(void *addr, u32 length, i32 prot, i32 flags, i32 fd, u32 offset) {
    (void)fd; (void)offset;
    process_t *proc = process_current();
    if (!proc || length == 0) {
        return (void *)-1;
    }

    if ((prot & 0x3) == 0) {
        prot = 3;
    }

    uintptr_t desired = (uintptr_t)addr;
    uintptr_t map_base = desired;
    if (!addr) {
        map_base = proc->brk;
        if (map_base < USER_HEAP_START) map_base = USER_HEAP_START;
    }

    if (map_base < USER_HEAP_START || map_base > USER_HEAP_END) {
        return (void *)-1;
    }

    u32 aligned_len = PAGE_ALIGN_UP(length);
    if (map_base + aligned_len > USER_HEAP_END) {
        return (void *)-1;
    }

    if (proc->mmap_count >= PROCESS_MAX_MMAPS) {
        return (void *)-1;
    }

    for (uintptr_t page = PAGE_ALIGN_DOWN(map_base); page < map_base + aligned_len; page += PAGE_SIZE) {
        uintptr_t phys = pmem_alloc(1);
        if (!phys) {
            for (uintptr_t mpage = PAGE_ALIGN_DOWN(map_base); mpage < page; mpage += PAGE_SIZE) {
                uintptr_t p = paging_get_physical(proc->pagedir, mpage);
                if (p) {
                    paging_unmap(proc->pagedir, mpage);
                    pmem_free(p & 0xFFFFF000ul, 1);
                }
            }
            return (void *)-1;
        }
        paging_map(proc->pagedir, page, phys, PAGE_PRESENT | PAGE_RW | PAGE_USER);
        memset((void *)page, 0, PAGE_SIZE);
    }

    proc->mmap_regions[proc->mmap_count].start = (u32)map_base;
    proc->mmap_regions[proc->mmap_count].length = aligned_len;
    proc->mmap_regions[proc->mmap_count].prot = (u32)prot;
    proc->mmap_regions[proc->mmap_count].flags = (u32)flags;
    proc->mmap_regions[proc->mmap_count].fd = (u32)(fd < 0 ? 0xFFFFFFFFu : (u32)fd);
    proc->mmap_regions[proc->mmap_count].offset = offset;
    proc->mmap_regions[proc->mmap_count].anonymous = 1;
    proc->mmap_count++;

    if (!addr) {
        proc->brk = map_base + aligned_len;
    }
    return (void *)map_base;
}

static i32 syscall_munmap(void *addr, u32 length) {
    process_t *proc = process_current();
    if (!proc || !addr || length == 0) {
        return -1;
    }

    uintptr_t start = PAGE_ALIGN_DOWN((uintptr_t)addr);
    uintptr_t end = PAGE_ALIGN_UP((uintptr_t)addr + length);
    if (start < USER_HEAP_START || end > USER_HEAP_END) {
        return -1;
    }

    for (uintptr_t page = start; page < end; page += PAGE_SIZE) {
        uintptr_t phys = paging_get_physical(proc->pagedir, page);
        if (!phys) {
            continue;
        }
        paging_unmap(proc->pagedir, page);
        pmem_free(phys & 0xFFFFF000ul, 1);
    }

    for (u32 i = 0; i < proc->mmap_count; i++) {
        if (proc->mmap_regions[i].start == (u32)start) {
            proc->mmap_regions[i].start = 0;
            proc->mmap_regions[i].length = 0;
            proc->mmap_regions[i].fd = 0;
            proc->mmap_regions[i].offset = 0;
            break;
        }
    }
    return 0;
}

static void *syscall_brk(void *addr) {
    process_t *proc = process_current();
    if (!proc) {
        return (void *)-1;
    }

    if (!addr) {
        return (void *)proc->brk;
    }

    uintptr_t new_brk = (uintptr_t)addr;
    if (new_brk < USER_HEAP_START || new_brk > USER_HEAP_END) {
        return (void *)-1;
    }

    uintptr_t cur = PAGE_ALIGN_UP(proc->brk);
    uintptr_t target = PAGE_ALIGN_UP(new_brk);
    if (target > cur) {
        for (uintptr_t page = cur; page < target; page += PAGE_SIZE) {
            uintptr_t phys = pmem_alloc(1);
            if (!phys) {
                return (void *)-1;
            }
            paging_map(proc->pagedir, page, phys, PAGE_PRESENT | PAGE_RW | PAGE_USER);
            memset((void *)page, 0, PAGE_SIZE);
        }
    }
    proc->brk = new_brk;
    return (void *)proc->brk;
}

/* Syscall wrappers for internal use */
u32 syscall_getpid(void) {
    u32 pid;
    __asm__ volatile("int $0x80" : "=a"(pid) : "a"(SYS_GETPID));
    return pid;
}

void syscall_init(void) {
    /* Register INT 0x80 as syscall handler */
    interrupt_install_handler(0x80, syscall_handler);
    kprintf("Syscall interface initialized\n");
}
