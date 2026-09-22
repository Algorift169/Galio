#ifndef GALIO_NET_LOOPBACK_H
#define GALIO_NET_LOOPBACK_H

#include "net/netdev.h"

int loopback_init(void);
net_device_t *loopback_device(void);

#endif /* GALIO_NET_LOOPBACK_H */
