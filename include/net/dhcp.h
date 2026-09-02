#ifndef INCLUDE_NET_DHCP_H
#define INCLUDE_NET_DHCP_H

#include "common.h"
#include "net/netdev.h"

int dhcp_start(void);
int dhcp_renew(net_device_t *dev);
int dhcp_release(net_device_t *dev);
const char *dhcp_state_name(net_dhcp_state_t state);
u32 dhcp_get_dns_server(void);

#endif