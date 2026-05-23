#ifndef INCLUDE_NET_UTIL_H
#define INCLUDE_NET_UTIL_H

#include "common.h"

void net_dump_packet(const uint8_t *data, u32 len);
void net_print_stats(void);

#endif
