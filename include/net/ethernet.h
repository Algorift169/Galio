#ifndef INCLUDE_NET_ETHERNET_H
#define INCLUDE_NET_ETHERNET_H

#include "common.h"
#include "net/packet.h"

#define ETH_ALEN 6
#define ETH_HDR_LEN 14
#define ETH_P_IP  0x0800
#define ETH_P_ARP 0x0806

struct eth_hdr {
    uint8_t dest[ETH_ALEN];
    uint8_t src[ETH_ALEN];
    u16 type;
} __attribute__((packed));

static inline u16 net_htons(u16 value) {
    return (value << 8) | (value >> 8);
}

static inline u16 net_ntohs(u16 value) {
    return (value << 8) | (value >> 8);
}

static inline u32 net_htonl(u32 value) {
    return ((value & 0x000000FFU) << 24) |
           ((value & 0x0000FF00U) << 8) |
           ((value & 0x00FF0000U) >> 8) |
           ((value & 0xFF000000U) >> 24);
}

static inline u32 net_ntohl(u32 value) {
    return net_htonl(value);
}

void ethernet_input(net_buf_t *buf);

#endif
