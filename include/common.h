#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed char s8;
typedef signed short s16;
typedef signed int s32;

/* Typedefs for clarity */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t  i32;

/* Kernel utility functions */
void *memset(void *s, int c, u32 n);
void *memcpy(void *dest, const void *src, u32 n);
void panic(const char *msg);
void assert_failed(const char *expr, const char *file, i32 line);
void stack_trace(void);

#define ASSERT(expr) ((expr) ? (void)0 : assert_failed(#expr, __FILE__, __LINE__))

/* Kernel status reporting */
void kernel_status(void);

#endif /* COMMON_H */
