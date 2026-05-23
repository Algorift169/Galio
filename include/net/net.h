#ifndef INCLUDE_NET_NET_H
#define INCLUDE_NET_NET_H

#include "net/netdev.h"
#include "net/packet.h"

void net_init(void);
void net_poll(void);
void net_input(net_buf_t *buf);
void net_print_devices(void);

#endif
