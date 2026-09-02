/*
 * Galio Kernel
 *
 * Copyright (C) 2026 S.M Israfil
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

#ifndef INCLUDE_NET_TCP_H
#define INCLUDE_NET_TCP_H

#include "common.h"
#include "net/packet.h"
#include "net/netdev.h"
#include "net/ipv4.h"

#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10

int tcp_init(void);
int tcp_connect(u32 dest_ip, u16 dest_port);
int tcp_send(u32 conn_id, const void *data, u32 length);
int tcp_receive(u32 conn_id, void *buffer, u32 buffer_len, u32 timeout_ms);
int tcp_close(u32 conn_id);
void tcp_input(net_buf_t *buf, struct ipv4_hdr *ip);
void tcp_poll(void);

#endif /* INCLUDE_NET_TCP_H */
