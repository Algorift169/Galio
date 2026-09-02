#ifndef INCLUDE_NET_DNS_H
#define INCLUDE_NET_DNS_H

#include "common.h"

int dns_resolve_a(const char *hostname, u32 *address);

#endif
