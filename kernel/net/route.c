#include "net/route.h"
#include "lib/string.h"

static net_route_t routes[NET_ROUTE_MAX];

void route_init(void) {
    memset(routes, 0, sizeof(routes));
}

int route_add(u32 network, u32 netmask, u32 gateway, net_device_t *device) {
    if (!device || (!netmask && (network || !gateway))) return -1;
    network &= netmask;
    for (u32 i = 0; i < NET_ROUTE_MAX; i++) {
        if (routes[i].used && routes[i].network == network && routes[i].netmask == netmask) {
            routes[i].gateway = gateway;
            routes[i].device = device;
            return 0;
        }
    }
    for (u32 i = 0; i < NET_ROUTE_MAX; i++) {
        if (!routes[i].used) {
            routes[i].used = 1;
            routes[i].network = network;
            routes[i].netmask = netmask;
            routes[i].gateway = gateway;
            routes[i].device = device;
            return 0;
        }
    }
    return -1;
}

int route_remove(u32 network, u32 netmask) {
    network &= netmask;
    for (u32 i = 0; i < NET_ROUTE_MAX; i++) {
        if (routes[i].used && routes[i].network == network && routes[i].netmask == netmask) {
            routes[i].used = 0;
            return 0;
        }
    }
    return -1;
}

net_device_t *route_lookup(u32 destination, u32 *next_hop) {
    net_route_t *best = NULL;
    u32 best_bits = 0;
    for (u32 i = 0; i < NET_ROUTE_MAX; i++) {
        if (!routes[i].used || (destination & routes[i].netmask) != routes[i].network) continue;
        u32 bits = 0;
        for (u32 mask = routes[i].netmask; mask; mask >>= 1) bits += mask & 1u;
        if (!best || bits > best_bits) {
            best = &routes[i];
            best_bits = bits;
        }
    }
    if (!best) return NULL;
    if (next_hop) *next_hop = best->gateway ? best->gateway : destination;
    return best->device;
}