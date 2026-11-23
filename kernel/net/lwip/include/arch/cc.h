/*
 * Galio Kernel - lwIP Architecture Configuration
 *
 * This file maps lwIP's platform abstraction layer to Galio's kernel types.
 * Required by lwIP's opt.h when LWIP_ARCH_CC_H is set.
 */

#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include "common.h"       /* Galio u8, u16, u32, u64 */
#include <stddef.h>
#include <stdint.h>

/* --------------- integer types ------------------------------------------- */
typedef uint8_t   u8_t;
typedef int8_t    s8_t;
typedef uint16_t  u16_t;
typedef int16_t   s16_t;
typedef uint32_t  u32_t;
typedef int32_t   s32_t;
typedef uintptr_t mem_ptr_t;

/* --------------- printf format macros ------------------------------------ */
#define U16_F  "u"
#define S16_F  "d"
#define X16_F  "x"
#define U32_F  "u"
#define S32_F  "d"
#define X32_F  "x"
#define SZT_F  "u"

/* --------------- endianness ---------------------------------------------- */
/* x86_64 is little-endian */
#define BYTE_ORDER LITTLE_ENDIAN

/* --------------- compiler hints ------------------------------------------ */
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_END
#define PACK_STRUCT_FIELD(x) x

#define LWIP_PLATFORM_ASSERT(x) \
    do { if (!(x)) { \
        extern void kprintf(const char *, ...); \
        kprintf("lwIP ASSERT: %s line %d\n", __FILE__, __LINE__); \
        for(;;) __asm__("hlt"); \
    } } while(0)

/* Use Galio's kprintf for diagnostics */
#ifndef LWIP_PLATFORM_DIAG
extern void kprintf(const char *fmt, ...);
#define LWIP_PLATFORM_DIAG(x) kprintf x
#endif

/* --------------- no hosted C library ------------------------------------- */
/* We provide our own mem/str functions via kernel/lib/string.c */
extern void *memset(void *s, int c, __SIZE_TYPE__ n);
extern void *memcpy(void *d, const void *s, __SIZE_TYPE__ n);
extern void *memmove(void *d, const void *s, __SIZE_TYPE__ n);
extern int   memcmp(const void *a, const void *b, __SIZE_TYPE__ n);

#define MEMCPY(dst, src, len)  memcpy(dst, src, len)
#define SMEMCPY(dst, src, len) memcpy(dst, src, len)

/*
 * Map libc allocator names to Galio's kernel heap.
 * lwIP's mem.c includes <stdlib.h> when MEM_LIBC_MALLOC=1; since we are
 * freestanding that header is empty.  Provide the symbols via macros so
 * the linker sees our kernel allocator instead of the hosted libc.
 */
extern void *kmalloc(__SIZE_TYPE__ size);
extern void  kfree(void *ptr);

static inline void *galio_calloc_cc(__SIZE_TYPE__ count, __SIZE_TYPE__ sz)
{
    __SIZE_TYPE__ total = count * sz;
    void *p = kmalloc(total);
    if (p) memset(p, 0, total);
    return p;
}

#ifndef malloc
#  define malloc(sz)          kmalloc(sz)
#endif
#ifndef free
#  define free(p)             kfree(p)
#endif
#ifndef calloc
#  define calloc(cnt, sz)     galio_calloc_cc(cnt, sz)
#endif

#endif /* LWIP_ARCH_CC_H */
