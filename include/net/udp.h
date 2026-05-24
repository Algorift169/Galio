#ifndef INCLUDE_NET_UDP_H
#define INCLUDE_NET_UDP_H

#include "common.h"
#include "net/packet.h"
#include "net/netdev.h"
#include "net/ipv4.h"

struct udp_hdr {
    u16 src_port;
    u16 dest_port;
    u16 len;
    u16 checksum;
} __attribute__((packed));

typedef void (*udp_receive_callback_t)(u32 src_ip, u16 src_port, u16 dest_port, const void *payload, u32 length);

int udp_init(void);
int udp_register_listener(u16 port, udp_receive_callback_t callback);
int udp_unregister_listener(u16 port);
int udp_send(u32 dest_ip, u16 dest_port, u16 src_port, const void *payload, u32 length);
void udp_input(net_buf_t *buf, struct ipv4_hdr *ip);

#endif /* INCLUDE_NET_UDP_H */
