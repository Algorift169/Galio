#ifndef SHELL_COMMON_H_WRAPPER
#define SHELL_COMMON_H_WRAPPER

#include <stdint.h>
#include <stddef.h>

/* Match the kernel ABI exactly so shell code and kernel headers do not conflict. */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef int32_t  i32;

void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void panic(const char *msg);
void assert_failed(const char *expr, const char *file, i32 line);
void stack_trace(void);

#define ASSERT(expr) ((expr) ? (void)0 : assert_failed(#expr, __FILE__, __LINE__))

void kernel_status(void);

#endif
