#ifndef SYSCALL_H
#define SYSCALL_H

#include "common.h"
#include "cpu.h"

#define SYSCALL_TRACE 1

/* Canonical Galio syscall numbers. Keep these stable for the current ABI. */
#define SYS_EXIT         1
#define SYS_WRITE        2
#define SYS_GETPID       3
#define SYS_FORK         4
#define SYS_EXEC         5
#define SYS_SLEEP        6
#define SYS_WAITPID      7
#define SYS_OPEN         8
#define SYS_READ         9
#define SYS_CLOSE        10
#define SYS_LSEEK        11
#define SYS_STAT         12
#define SYS_MMAP         13
#define SYS_MUNMAP       14
#define SYS_BRK          15
#define SYS_EXECVE       16
#define SYS_PIPE         17
#define SYS_DUP          18
#define SYS_DUP2         19
#define SYS_CHDIR        20
#define SYS_GETCWD       21
#define SYS_TIME         22
#define SYS_GETTIMEOFDAY 23
#define SYS_GETUID       24
#define SYS_GETGID       25
#define SYS_GETPPID      26
#define SYS_KILL         27
#define SYS_WAIT         28

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

/* Pipe support for process I/O */
u32 pipe_read_fd(u32 handle, void *buffer, u32 size);
u32 pipe_write_fd(u32 handle, const void *buffer, u32 size);
u32 pipe_retain_handle(u32 handle);
u32 pipe_close_handle(u32 handle);

#endif /* SYSCALL_H */
