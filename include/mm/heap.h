/*
 * Galio Kernel
 *
 * Copyright (C) 2026 S.M Israfil
 *
 * This file is part of Galio.
 *
 * Galio is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * Galio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Galio. If not, see <https://www.gnu.org/licenses/>.
 */

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

/* Memory statistics functions */
u32 heap_get_used_memory(void);
u32 heap_get_total_memory(void);

#endif /* HEAP_H */
