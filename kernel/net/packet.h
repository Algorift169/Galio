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

#ifndef NET_PACKET_H
#define NET_PACKET_H

#include "common.h"

typedef struct net_device net_device_t;

#define NET_BUF_CSUM_IP_VALID    0x0001u
#define NET_BUF_CSUM_TCP_VALID   0x0002u
#define NET_BUF_CSUM_UDP_VALID   0x0004u
#define NET_BUF_CSUM_IP_OFFLOAD  0x0010u
#define NET_BUF_CSUM_TCP_OFFLOAD 0x0020u
#define NET_BUF_CSUM_UDP_OFFLOAD 0x0040u

typedef struct net_buf {
    uint8_t *data;      /* pointer to payload */
    u32 len;            /* payload length */
    u32 headroom;       /* reserved headroom */
    u16 csum_flags;     /* checksum validation/offload metadata */
    u16 csum;           /* partial checksum seed when offloading */
    net_device_t *dev;  /* originating/receiving device */
    struct net_buf *next;
} net_buf_t;

/* Allocate a net buffer with 'size' payload bytes + optional headroom */
net_buf_t *net_buf_alloc(u32 size, u32 headroom);
void net_buf_free(net_buf_t *buf);

/* Convenience: copy data into an allocated buffer */
net_buf_t *net_buf_clone_from_data(const void *data, u32 len);

#endif /* NET_PACKET_H */
