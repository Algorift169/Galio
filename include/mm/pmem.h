#ifndef PMEM_H
#define PMEM_H

#include "common.h"
#include <stddef.h>

typedef struct {
    u32 size;
    u32 addr_low;
    u32 addr_high;
    u32 len_low;
    u32 len_high;
    u32 type;
} mmap_entry_t;

#define MMAP_AVAILABLE 1
#define MMAP_RESERVED 2

void pmem_init(u32 mmap_addr, u32 mmap_length);
u32 pmem_alloc(size_t num_frames);
void pmem_free(u32 addr, size_t num_frames);
void pmem_refcount_inc(u32 addr);
void pmem_refcount_dec(u32 addr);
u32 pmem_get_refcount(u32 addr);
u32 pmem_get_total(void);
u32 pmem_get_used(void);
u32 pmem_get_free(void);
void pmem_claim(u32 addr, size_t num_frames);

#endif /* PMEM_H */
