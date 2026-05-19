#ifndef SYSCALL_H
#define SYSCALL_H

#include "common.h"
#include "cpu.h"

/* Syscall numbers */
#define SYS_EXIT     1
#define SYS_WRITE    2
#define SYS_GETPID   3
#define SYS_FORK     4
#define SYS_EXEC     5
#define SYS_SLEEP    6
#define SYS_WAITPID  7
#define SYS_OPEN     8
#define SYS_READ     9
#define SYS_CLOSE    10
#define SYS_LSEEK    11
#define SYS_STAT     12
#define SYS_MMAP     13
#define SYS_MUNMAP   14
#define SYS_BRK      15

/* Initialize syscall interface (register INT 0x80 handler) */
void syscall_init(void);

/* Syscall wrapper functions for internal use */
u32 syscall_getpid(void);
void syscall_sleep(u32 ms);

#endif /* SYSCALL_H */