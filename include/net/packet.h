#ifndef INCLUDE_NET_PACKET_H
#define INCLUDE_NET_PACKET_H

#include "common.h"

typedef struct net_device net_device_t;

typedef struct net_buf {
    uint8_t *data;
    u32 len;
    u32 headroom;
    net_device_t *dev;
    struct net_buf *next;
} net_buf_t;

net_buf_t *net_buf_alloc(u32 size, u32 headroom);
void net_buf_free(net_buf_t *buf);
net_buf_t *net_buf_clone_from_data(const void *data, u32 len);

#endif
