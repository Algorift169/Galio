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
