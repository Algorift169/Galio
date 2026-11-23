/*
 * Galio Kernel - lwIP netif adapter public API
 */

#ifndef GALIO_LWIP_NETIF_H
#define GALIO_LWIP_NETIF_H

#include "net/packet.h"
#include "net/netdev.h"

/* Initialise lwIP and register the first e1000 device as the default interface.
 * Called from net_init() after the e1000 driver has been registered. */
int lwip_netif_init_all(void);

/* Apply a static IPv4 configuration to the active lwIP interface. */
int lwip_netif_set_ipv4(net_device_t *dev, u32 addr, u32 netmask, u32 gateway);

/* Notify lwIP when the active device's physical link changes. */
void lwip_netif_set_link(net_device_t *dev, int link_up);

/* Query or clear the active lwIP interface's ARP cache. */
int lwip_arp_resolve(net_device_t *dev, u32 addr, u8 mac[6]);
void lwip_arp_flush(net_device_t *dev);

/* Inject a received Ethernet frame into the lwIP stack.
 * Called from Galio's net_input() for every packet received from the NIC. */
void lwip_netif_input(net_buf_t *buf);

/* Called once per PIT timer tick to advance lwIP's timeout machinery. */
void lwip_timer_tick(void);

/* Return the primary lwIP netif, or NULL if not yet initialised. */
struct netif *lwip_get_netif(void);

/* Return 1 if the lwIP stack has been initialised. */
int lwip_is_ready(void);

#endif /* GALIO_LWIP_NETIF_H */
