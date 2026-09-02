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

#include "packet.h"
#include "mm/heap.h"
#include "lib/kprintf.h"
#include "lib/string.h"

net_buf_t *net_buf_alloc(u32 size, u32 headroom) {
    if (headroom > 0xFFFFFFFFu - size) {
        return NULL;
    }

    net_buf_t *nb = kmalloc(sizeof(net_buf_t));
    if (!nb) return NULL;
    nb->headroom = headroom;
    nb->len = size;
    nb->next = NULL;
    nb->dev = NULL;
    nb->data = kmalloc(headroom + size);
    if (!nb->data) {
        kfree(nb);
        return NULL;
    }
    /* place payload after headroom */
    nb->data += headroom;
    return nb;
}

void net_buf_free(net_buf_t *buf) {
    if (!buf) return;
    /* data pointer may have been offset by headroom; compute original ptr */
    if (buf->data) {
        uint8_t *orig = buf->data - buf->headroom;
        kfree(orig);
    }
    kfree(buf);
}

net_buf_t *net_buf_clone_from_data(const void *data, u32 len) {
    net_buf_t *nb = net_buf_alloc(len, 0);
    if (!nb) return NULL;
    memcpy(nb->data, data, len);
    nb->len = len;
    return nb;
}
