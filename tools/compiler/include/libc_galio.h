#ifndef GC_LIBC_GALIO_H
#define GC_LIBC_GALIO_H

#include <stdint.h>

/* Galio i386 INT 0x80 syscall numbers. */
#define GALIO_SYS_EXIT       1
#define GALIO_SYS_FORK       2
#define GALIO_SYS_READ       3
#define GALIO_SYS_WRITE      4
#define GALIO_SYS_OPEN       5
#define GALIO_SYS_CLOSE      6
#define GALIO_SYS_WAITPID    7
#define GALIO_SYS_EXECVE     11
#define GALIO_SYS_CHDIR      12
#define GALIO_SYS_TIME       13
#define GALIO_SYS_LSEEK      19
#define GALIO_SYS_GETPID     20
#define GALIO_SYS_ACCESS     33
#define GALIO_SYS_BRK        45
#define GALIO_SYS_MMAP       90
#define GALIO_SYS_MUNMAP     91
#define GALIO_SYS_STAT       106
#define GALIO_SYS_GETCWD     183
#define GALIO_SYS_NANOSLEEP  162

#define GALIO_O_RDONLY  0x0000
#define GALIO_O_WRONLY  0x0001
#define GALIO_O_RDWR    0x0002
#define GALIO_O_CREAT   0x0040
#define GALIO_O_TRUNC   0x0200
#define GALIO_O_APPEND  0x0400

static inline int galio_syscall0(int n) { int ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(n) : "memory"); return ret; }
static inline int galio_syscall1(int n, int a1) { int ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(n), "b"(a1) : "memory"); return ret; }
static inline int galio_syscall2(int n, int a1, int a2) { int ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(n), "b"(a1), "c"(a2) : "memory"); return ret; }
static inline int galio_syscall3(int n, int a1, int a2, int a3) { int ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(n), "b"(a1), "c"(a2), "d"(a3) : "memory"); return ret; }
static inline void galio_exit(int code) { galio_syscall1(GALIO_SYS_EXIT, code); for (;;) {} }
static inline int galio_read(int fd, void *buf, int count) { return galio_syscall3(GALIO_SYS_READ, fd, (int)(uintptr_t)buf, count); }
static inline int galio_write(int fd, const void *buf, int count) { return galio_syscall3(GALIO_SYS_WRITE, fd, (int)(uintptr_t)buf, count); }
static inline int galio_open(const char *path, int flags, int mode) { return galio_syscall3(GALIO_SYS_OPEN, (int)(uintptr_t)path, flags, mode); }
static inline int galio_close(int fd) { return galio_syscall1(GALIO_SYS_CLOSE, fd); }
static inline int galio_brk(void *addr) { return galio_syscall1(GALIO_SYS_BRK, (int)(uintptr_t)addr); }
static inline int galio_munmap(void *addr, int length) { return galio_syscall2(GALIO_SYS_MUNMAP, (int)(uintptr_t)addr, length); }
static inline int galio_strlen(const char *s) { int n = 0; while (s && s[n]) n++; return n; }
static inline void *galio_memcpy(void *dst, const void *src, int n) { char *d = (char *)dst; const char *s = (const char *)src; while (n-- > 0) *d++ = *s++; return dst; }
static inline void *galio_memset(void *dst, int value, int n) { char *d = (char *)dst; while (n-- > 0) *d++ = (char)value; return dst; }

#endif
