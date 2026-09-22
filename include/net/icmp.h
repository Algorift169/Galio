#ifndef GALIO_NET_ICMP_H
#define GALIO_NET_ICMP_H

#include "net/ipv4.h"

int icmpv4_send_echo(net_device_t *device, u32 destination, u16 identifier,
                     u16 sequence, const void *payload, u32 payload_length);
void icmpv4_input(net_buf_t *buffer, struct ipv4_hdr *ip);

#endif /* GALIO_NET_ICMP_H */
