#ifndef INCLUDE_NET_DHCP_H
#define INCLUDE_NET_DHCP_H

#include "common.h"

int dhcp_start(void);
u32 dhcp_get_dns_server(void);

#endif