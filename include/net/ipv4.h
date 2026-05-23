#ifndef INCLUDE_NET_IPV4_H
#define INCLUDE_NET_IPV4_H

#include "common.h"
#include "net/packet.h"
#include "net/netdev.h"
#include "net/ethernet.h"

#define IPV4_PROTO_ICMP 1
#define IPV4_PROTO_TCP  6
#define IPV4_PROTO_UDP  17
#define ICMP_ECHO_REQUEST 8
#define ICMP_ECHO_REPLY   0

struct ipv4_hdr {
    u8 version_ihl;
    u8 tos;
    u16 tot_len;
    u16 id;
    u16 frag_off;
    u8 ttl;
    u8 protocol;
    u16 checksum;
    u32 src;
    u32 dest;
} __attribute__((packed));

struct icmp_hdr {
    u8 type;
    u8 code;
    u16 checksum;
    u16 id;
    u16 sequence;
} __attribute__((packed));

void ipv4_init(void);
void ipv4_input(net_buf_t *buf);
u16 ipv4_checksum(const void *data, u32 len);

#endif
