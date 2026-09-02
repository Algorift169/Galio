#ifndef INCLUDE_NET_ROUTE_H
#define INCLUDE_NET_ROUTE_H

#include "common.h"
#include "net/netdev.h"

#define NET_ROUTE_MAX 16u

typedef struct {
    u32 network;
    u32 netmask;
    u32 gateway;
    net_device_t *device;
    u8 used;
} net_route_t;

void route_init(void);
int route_add(u32 network, u32 netmask, u32 gateway, net_device_t *device);
int route_remove(u32 network, u32 netmask);
net_device_t *route_lookup(u32 destination, u32 *next_hop);

#endif