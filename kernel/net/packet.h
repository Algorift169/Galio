#ifndef NET_PACKET_H
#define NET_PACKET_H

#include "common.h"

typedef struct net_device net_device_t;

typedef struct net_buf {
    uint8_t *data;      /* pointer to payload */
    u32 len;            /* payload length */
    u32 headroom;       /* reserved headroom */
    net_device_t *dev;  /* originating/receiving device */
    struct net_buf *next;
} net_buf_t;

/* Allocate a net buffer with 'size' payload bytes + optional headroom */
net_buf_t *net_buf_alloc(u32 size, u32 headroom);
void net_buf_free(net_buf_t *buf);

/* Convenience: copy data into an allocated buffer */
net_buf_t *net_buf_clone_from_data(const void *data, u32 len);

#endif /* NET_PACKET_H */
