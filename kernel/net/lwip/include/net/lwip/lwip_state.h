/*
 * Galio Kernel - lwIP shared state: ping helper and DNS resolver
 *
 * Provides kernel-facing wrappers for lwIP ICMP ping and DNS that
 * the shell commands (net ping, net dns) can call.
 */

#ifndef GALIO_LWIP_STATE_H
#define GALIO_LWIP_STATE_H

#include "common.h"

/*
 * Synchronous (polling) ICMP ping.
 * Sends `count` echo requests to `dest_ip` (host byte order).
 * Prints results directly via kprintf.
 * Returns the number of replies received.
 *
 * Uses lwIP raw API internally. Because we are in NO_SYS mode this call
 * polls the network stack until a reply arrives or the timeout expires.
 */
int lwip_ping(u32 dest_ip, u32 count, u32 timeout_ms);

/*
 * DNS name resolution (blocking poll loop).
 * Resolves `hostname` and stores the result in `*out_ip` (host byte order).
 * Returns 0 on success, -1 on failure or timeout.
 */
int lwip_dns_resolve(const char *hostname, u32 *out_ip);

#endif /* GALIO_LWIP_STATE_H */
