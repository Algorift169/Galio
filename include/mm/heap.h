#ifndef HEAP_H
#define HEAP_H

#include "common.h"
#include <stddef.h>

typedef struct slab_cache {
    size_t object_size;
    u32 object_count;
    void *memory;
    void *free_list;
} slab_cache_t;

void heap_init(void);
void *kmalloc(size_t size);
void *kcalloc(size_t nmemb, size_t size);
void *krealloc(void *ptr, size_t size);
void kfree(void *ptr);
void *vmalloc(size_t size);
void vfree(void *ptr);
void *dma_alloc(size_t size);
void dma_free(void *ptr, size_t size);
void slab_cache_init(slab_cache_t *cache, size_t object_size, u32 object_count);
void *slab_alloc(slab_cache_t *cache);
void slab_free(slab_cache_t *cache, void *ptr);

#endif /* HEAP_H */
