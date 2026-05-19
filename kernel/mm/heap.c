/* heap.c - Simple kernel heap allocator (uses paging and pmem) */
/* heap.c - Simple kernel heap allocator (uses paging and pmem) */
#include "heap.h"
#include "pmem.h"
#include "paging.h"
#include "kprintf.h"

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#define HEAP_START      0x500000
#define HEAP_MAX_SIZE   0x1000000   /* 16 MB */
#define MIN_BLOCK_SIZE  32


typedef struct block {
    size_t size;
    struct block *next;
    struct block *prev;
    u8 used;
    u32 magic;  /* 0xDEADBEEF for sanity */
} block_t;

#define MAGIC_VAL 0xDEADBEEF
#define BLOCK_HEADER_SIZE sizeof(block_t)

static block_t *free_list = NULL;
static u32 heap_top = HEAP_START;

void heap_init(void) {
    kprintf("Initializing kernel heap at 0x%x\n", HEAP_START);
    /* The first block covers the whole heap */
    free_list = (block_t *)HEAP_START;
    free_list->size = HEAP_MAX_SIZE - BLOCK_HEADER_SIZE;
    free_list->next = NULL;
    free_list->prev = NULL;
    free_list->used = 0;
    free_list->magic = MAGIC_VAL;
    heap_top = HEAP_START + HEAP_MAX_SIZE;
    kprintf("Heap ready: top = 0x%x\n", heap_top);
}

static void split_block(block_t *block, size_t size) {
    if (block->size < size + BLOCK_HEADER_SIZE + MIN_BLOCK_SIZE)
        return;
    block_t *new_block = (block_t *)((u32)block + BLOCK_HEADER_SIZE + size);
    new_block->size = block->size - size - BLOCK_HEADER_SIZE;
    new_block->used = 0;
    new_block->next = block->next;
    new_block->prev = block;
    new_block->magic = MAGIC_VAL;
    block->size = size;
    block->next = new_block;
    if (new_block->next)
        new_block->next->prev = new_block;
}

static void coalesce_blocks(block_t *block) {
    block_t *next = block->next;
    if (next && !next->used) {
        block->size += BLOCK_HEADER_SIZE + next->size;
        block->next = next->next;
        if (next->next) next->next->prev = block;
    }
    block_t *prev = block->prev;
    if (prev && !prev->used) {
        prev->size += BLOCK_HEADER_SIZE + block->size;
        prev->next = block->next;
        if (block->next) block->next->prev = prev;
    }
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;
    size = (size + 7) & ~7; /* align to 8 bytes */

    block_t *curr = free_list;
    while (curr) {
        if (!curr->used && curr->size >= size) {
            split_block(curr, size);
            curr->used = 1;
            return (void *)((u32)curr + BLOCK_HEADER_SIZE);
        }
        curr = curr->next;
    }
    kprintf("kmalloc: Out of memory (size=%u)\n", size);
    return NULL;
}

void kfree(void *ptr) {
    if (!ptr) return;
    block_t *block = (block_t *)((u32)ptr - BLOCK_HEADER_SIZE);
    if (block->magic != MAGIC_VAL) {
        kprintf("kfree: Invalid pointer 0x%x\n", (u32)ptr);
        return;
    }
    block->used = 0;
    coalesce_blocks(block);
}

void *kcalloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *ptr = kmalloc(total);
    if (ptr) __builtin_memset(ptr, 0, total);
    return ptr;
}

void *krealloc(void *ptr, size_t new_size) {
    if (!ptr) return kmalloc(new_size);
    if (new_size == 0) { kfree(ptr); return NULL; }
    block_t *block = (block_t *)((u32)ptr - BLOCK_HEADER_SIZE);
    if (block->size >= new_size) return ptr;
    void *new_ptr = kmalloc(new_size);
    if (!new_ptr) return NULL;
    __builtin_memcpy(new_ptr, ptr, block->size);
    kfree(ptr);
    return new_ptr;
}

void *vmalloc(size_t size) {
    return kmalloc(size);
}

void vfree(void *ptr) {
    kfree(ptr);
}

void *dma_alloc(size_t size) {
    if (size == 0) return NULL;
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    u32 frames = size / PAGE_SIZE;
    u32 phys = pmem_alloc(frames);
    return phys ? (void *)phys : NULL;
}

void dma_free(void *ptr, size_t size) {
    if (!ptr || size == 0) return;
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    pmem_free((u32)ptr, size / PAGE_SIZE);
}

void slab_cache_init(slab_cache_t *cache, size_t object_size, u32 object_count) {
    if (!cache || object_size == 0 || object_count == 0) return;
    cache->object_size = (object_size + 7) & ~7;
    cache->object_count = object_count;
    cache->memory = kmalloc(cache->object_size * object_count);
    cache->free_list = NULL;
    if (!cache->memory) return;

    u8 *ptr = (u8 *)cache->memory;
    for (u32 i = 0; i < object_count; i++) {
        *(void **)ptr = cache->free_list;
        cache->free_list = ptr;
        ptr += cache->object_size;
    }
}

void *slab_alloc(slab_cache_t *cache) {
    if (!cache || !cache->free_list) return NULL;
    void *obj = cache->free_list;
    cache->free_list = *(void **)cache->free_list;
    return obj;
}

void slab_free(slab_cache_t *cache, void *ptr) {
    if (!cache || !ptr) return;
    *(void **)ptr = cache->free_list;
    cache->free_list = ptr;
}