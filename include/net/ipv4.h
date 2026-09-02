/*
 * Galio Kernel
 *
 * Copyright (C) 2026 Israfil [Your Legal Name]
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

int ipv4_send(net_device_t *dev, u32 dest_ip, u8 proto, const void *payload, u32 payload_len);

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
