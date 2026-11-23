#ifndef INCLUDE_NET_DHCP_H
#define INCLUDE_NET_DHCP_H

#include "common.h"
#include "net/netdev.h"

int galio_dhcp_start(void);
int galio_dhcp_renew(net_device_t *dev);
int galio_dhcp_release(net_device_t *dev);
const char *dhcp_state_name(net_dhcp_state_t state);
u32 dhcp_get_dns_server(void);

#define dhcp_start   galio_dhcp_start
#define dhcp_renew   galio_dhcp_renew
#define dhcp_release galio_dhcp_release

#endif
