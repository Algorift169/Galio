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

#ifndef INCLUDE_NET_NETDEV_H
#define INCLUDE_NET_NETDEV_H

#include "common.h"
#include "net/packet.h"

#define NET_NAME_LEN 16
#define NETIF_UP        0x1
#define NETIF_RUNNING   0x2

typedef enum {
    NET_DHCP_INIT = 0,
    NET_DHCP_SELECTING,
    NET_DHCP_REQUESTING,
    NET_DHCP_BOUND,
    NET_DHCP_RENEWING,
    NET_DHCP_REBINDING,
    NET_DHCP_EXPIRED,
    NET_DHCP_ERROR
} net_dhcp_state_t;

typedef struct net_device {
    char name[NET_NAME_LEN];
    uint8_t mac[6];
    u32 mtu;
    u32 flags;
    void *priv;
    int (*open)(struct net_device *dev);
    int (*stop)(struct net_device *dev);
    int (*tx)(struct net_device *dev, net_buf_t *buf);
    int (*poll)(struct net_device *dev);
    int (*get_link)(struct net_device *dev);
    int (*get_speed)(struct net_device *dev);
    int (*set_channel)(struct net_device *dev, u8 channel);
    u32 ip_addr;
    u32 netmask;
    u32 broadcast;
    u32 gateway;
    u32 dns_servers[2];
    u32 dhcp_server;
    u32 lease_duration;
    u32 lease_start_ticks;
    u32 lease_t1_ticks;
    u32 lease_t2_ticks;
    u32 lease_expiry_ticks;
    net_dhcp_state_t dhcp_state;
    struct net_device *next;
    u32 rx_packets;
    u32 tx_packets;
    u32 rx_bytes;
    u32 tx_bytes;
    u32 rx_dropped;
    u32 tx_dropped;
    u32 rx_errors;
    u32 tx_errors;
} net_device_t;

int netdev_register(net_device_t *dev);
int netdev_unregister(net_device_t *dev);
net_device_t *netdev_get_by_name(const char *name);
net_device_t *netdev_route(u32 dest_ip);
int netdev_send_skb(net_device_t *dev, net_buf_t *buf);
int netdev_receive_skb(net_device_t *dev, net_buf_t *buf);
void netdev_set_ipv4(net_device_t *dev, u32 addr, u32 netmask, u32 gateway);
void netdev_clear_ipv4(net_device_t *dev);
int netdev_get_link(net_device_t *dev);
net_device_t *netdev_first(void);
net_device_t *netdev_next(net_device_t *cur);
void net_core_init(void);

#endif
