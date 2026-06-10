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
