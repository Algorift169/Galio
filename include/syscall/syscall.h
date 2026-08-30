#ifndef SYSCALL_H
#define SYSCALL_H

#include "common.h"
#include "cpu.h"

#define SYSCALL_TRACE 1

/*
 * Compatibility syscall number space, with Galio backward-compatibility aliases
 * kept for existing user code and shell helpers.
 *
 * The kernel uses the classic INT 0x80 ABI, while the numbers remain aligned to
 * a familiar interface for portable userspace tooling.
 */
#define SYS_READ         0
#define SYS_WRITE        1
#define SYS_OPEN         2
#define SYS_CLOSE        3
#define SYS_STAT         4
#define SYS_FSTAT        5
#define SYS_LSTAT        6
#define SYS_POLL         7
#define SYS_LSEEK        8
#define SYS_MMAP         9
#define SYS_MPROTECT    10
#define SYS_MUNMAP      11
#define SYS_BRK         12
#define SYS_RT_SIGACTION 13
#define SYS_RT_SIGPROCMASK 14
#define SYS_RT_SIGRETURN 15
#define SYS_IOCTL       16
#define SYS_PREAD64     17
#define SYS_PWRITE64    18
#define SYS_READV       19
#define SYS_WRITEV      20
#define SYS_ACCESS      21
#define SYS_PIPE        22
#define SYS_SELECT      23
#define SYS_SCHED_YIELD 24
#define SYS_DUP         32
#define SYS_DUP2        33
#define SYS_PAUSE       34
#define SYS_NANOSLEEP   35
#define SYS_GETPID      39
#define SYS_SOCKET      41
#define SYS_CONNECT     42
#define SYS_ACCEPT      43
#define SYS_SENDTO      44
#define SYS_RECVFROM    45
#define SYS_SENDMSG     46
#define SYS_RECVMSG     47
#define SYS_SHUTDOWN    48
#define SYS_BIND        49
#define SYS_LISTEN      50
#define SYS_GETSOCKNAME 51
#define SYS_GETPEERNAME 52
#define SYS_SOCKETPAIR  53
#define SYS_SETSOCKOPT  54
#define SYS_GETSOCKOPT  55
#define SYS_CLONE       56
#define SYS_FORK        57
#define SYS_VFORK       58
#define SYS_EXECVE      59
#define SYS_EXEC        250
#define SYS_EXIT        60
#define SYS_WAIT4       61
#define SYS_WAITPID     251
#define SYS_WAIT        252
#define SYS_KILL        62
#define SYS_UNAME       63
#define SYS_SEMGET      64
#define SYS_SEMOP       65
#define SYS_SEMCTL      66
#define SYS_SHMDT       67
#define SYS_SHMGET      68
#define SYS_SHMAT       69
#define SYS_SHMCTL      71
#define SYS_GETTIMEOFDAY 96
#define SYS_GETUID      102
#define SYS_GETEUID     107
#define SYS_GETEGID     108
#define SYS_GETGID      104
#define SYS_GETPPID     110
#define SYS_GETCWD      79
#define SYS_CHDIR       80
#define SYS_READLINK    89
#define SYS_MMAP2       253
#define SYS_CLOCK_GETTIME 228
#define SYS_SYSINFO     179
#define SYS_IOCTL2      254
#define SYS_TIME        201
#define SYS_SLEEP       202

/* Legacy Galio compatibility aliases retained for older code paths. */
#define SYS_GALIO_EXIT         1
#define SYS_GALIO_WRITE        2
#define SYS_GALIO_GETPID       3
#define SYS_GALIO_FORK         4
#define SYS_GALIO_EXEC         5
#define SYS_GALIO_SLEEP        6
#define SYS_GALIO_WAITPID      7
#define SYS_GALIO_OPEN         8
#define SYS_GALIO_READ         9
#define SYS_GALIO_CLOSE        10
#define SYS_GALIO_LSEEK        11
#define SYS_GALIO_STAT         12
#define SYS_GALIO_MMAP         13
#define SYS_GALIO_MUNMAP       14
#define SYS_GALIO_BRK          15
#define SYS_GALIO_EXECVE       16
#define SYS_GALIO_PIPE         17
#define SYS_GALIO_DUP          18
#define SYS_GALIO_DUP2         19
#define SYS_GALIO_CHDIR        20
#define SYS_GALIO_GETCWD       21
#define SYS_GALIO_TIME         22
#define SYS_GALIO_GETTIMEOFDAY 23
#define SYS_GALIO_GETUID       24
#define SYS_GALIO_GETGID       25
#define SYS_GALIO_GETPPID      26
#define SYS_GALIO_KILL         27
#define SYS_GALIO_WAIT         28

#define PIPE_FD_FLAG     0x80000000u
#define PIPE_READ_FLAG   0x40000000u
#define PIPE_WRITE_FLAG  0x20000000u
#define PIPE_ID_MASK     0x0000000Fu
#define PIPE_MAX         16u
#define PIPE_BUFFER_SIZE 1024u

#ifndef __timeval_defined
#ifndef _STRUCT_TIMEVAL
#define _STRUCT_TIMEVAL
struct timeval {
    u32 tv_sec;
    u32 tv_usec;
};
#endif
#define __timeval_defined 1
#endif
#define SYS_TIMEVAL_STRUCT_DEFINED

struct timespec {
    long tv_sec;
    long tv_nsec;
};

struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

struct stat {
    u64 st_dev;
    u64 st_ino;
    u64 st_nlink;
    u32 st_mode;
    u32 st_uid;
    u32 st_gid;
    u32 __pad0;
    u64 st_size;
    u64 st_blksize;
    u64 st_blocks;
    u64 st_atime;
    u64 st_mtime;
    u64 st_ctime;
};

/* Initialize syscall interface (register INT 0x80 handler) */
void syscall_init(void);

/* Syscall wrapper functions for internal use */
u32 syscall_getpid(void);
void syscall_sleep(u32 ms);

/* User pointer validation for syscall entry points */
u8 validate_user_ptr(const void *ptr, u8 write);
u8 validate_user_buffer(const void *ptr, u32 length, u8 write);
u8 validate_user_string(const char *str, u32 max_length);

/* Extended syscall implementations */
uintptr_t syscall_execve(const char *path, char *const argv[], char *const envp[]);
i32 syscall_pipe(i32 pipefd[2]);
u32 syscall_dup(u32 oldfd);
i32 syscall_dup2(u32 oldfd, u32 newfd);
i32 syscall_chdir(const char *path);
u32 syscall_getcwd(char *buffer, u32 size);
u32 syscall_time(void);
i32 syscall_gettimeofday(struct timeval *tv, void *tz);
u32 syscall_getuid(void);
u32 syscall_getgid(void);
i32 syscall_geteuid(void);
i32 syscall_getegid(void);
i32 syscall_getppid(void);
i32 syscall_kill(u32 pid, i32 sig);
i32 syscall_uname(struct utsname *buf);
i32 syscall_access(const char *path, i32 mode);
i32 syscall_fstat(int fd, struct stat *statbuf);
i32 syscall_readlink(const char *path, char *buf, u32 bufsize);
i64 syscall_clock_gettime(i32 clk_id, void *tp);
i32 syscall_nanosleep(const struct timespec *req, struct timespec *rem);
i64 syscall_ioctl(u32 fd, u32 cmd, void *arg);
i64 syscall_poll(void *fds, u32 nfds, i32 timeout);

/* Extended syscall declarations */
i32 syscall_lstat(const char *path, struct stat *statbuf);
i32 syscall_mprotect(void *addr, u32 len, i32 prot);
i64 syscall_pread64(u32 fd, void *buf, u32 count, u64 offset);
i64 syscall_pwrite64(u32 fd, const void *buf, u32 count, u64 offset);
i32 syscall_readv(u32 fd, void *iov, i32 iovcnt);
i32 syscall_writev(u32 fd, const void *iov, i32 iovcnt);
i32 syscall_select(i32 nfds, void *readfds, void *writefds, void *exceptfds, struct timespec *timeout);
i32 syscall_sched_yield(void);
i32 syscall_pause(void);
i32 syscall_socket(i32 domain, i32 type, i32 protocol);
i32 syscall_connect(i32 sockfd, const void *addr, u32 addrlen);
i32 syscall_accept(i32 sockfd, void *addr, u32 *addrlen);
i32 syscall_sendto(i32 sockfd, const void *buf, u32 len, i32 flags, const void *dest_addr, u32 addrlen);
i32 syscall_recvfrom(i32 sockfd, void *buf, u32 len, i32 flags, void *src_addr, u32 *addrlen);
i32 syscall_sendmsg(i32 sockfd, const void *msg, i32 flags);
i32 syscall_recvmsg(i32 sockfd, void *msg, i32 flags);
i32 syscall_shutdown(i32 sockfd, i32 how);
i32 syscall_bind(i32 sockfd, const void *addr, u32 addrlen);
i32 syscall_listen(i32 sockfd, i32 backlog);
i32 syscall_getsockname(i32 sockfd, void *addr, u32 *addrlen);
i32 syscall_getpeername(i32 sockfd, void *addr, u32 *addrlen);
i32 syscall_socketpair(i32 domain, i32 type, i32 protocol, i32 *sv);
i32 syscall_setsockopt(i32 sockfd, i32 level, i32 optname, const void *optval, u32 optlen);
i32 syscall_getsockopt(i32 sockfd, i32 level, i32 optname, void *optval, u32 *optlen);
u32 syscall_clone(u32 flags, void *stack, i32 *ptid, void *tls, i32 *ctid);
u32 syscall_vfork(void);
i32 syscall_wait4(i32 pid, i32 *wstatus, i32 options, void *rusage);
i32 syscall_semget(u32 key, i32 nsems, i32 semflg);
i32 syscall_semop(i32 semid, void *sops, u32 nsops);
i32 syscall_semctl(i32 semid, i32 semnum, i32 cmd, void *arg);
void *syscall_shmat(i32 shmid, const void *shmaddr, i32 shmflg);
i32 syscall_shmdt(const void *shmaddr);
i32 syscall_shmget(u32 key, u32 size, i32 shmflg);
i32 syscall_shmctl(i32 shmid, i32 cmd, void *buf);
i32 syscall_rt_sigaction(i32 sig, const void *act, void *oldact, u32 sigsetsize);
i32 syscall_rt_sigprocmask(i32 how, const void *set, void *oldset, u32 sigsetsize);
i32 syscall_rt_sigreturn(void);
i32 syscall_sysinfo(void *info);

/* Pipe support for process I/O */
u32 pipe_read_fd(u32 handle, void *buffer, u32 size);
u32 pipe_write_fd(u32 handle, const void *buffer, u32 size);
u32 pipe_retain_handle(u32 handle);
u32 pipe_close_handle(u32 handle);

#endif /* SYSCALL_H */
