/*
 * Galio Kernel — DNS resolver (lwIP-backed)
 *
 * dns_resolve_a() is now a thin shim over lwip_dns_resolve().
 * The old UDP-based hand-rolled DNS implementation was removed when
 * kernel/net/udp.c was replaced by lwIP's core/udp.c.
 *
 * Status: Compiled. Not yet runtime-tested.
 */

#include "net/dns.h"
#include "net/net.h"
#include "net/lwip/lwip_netif.h"
#include "net/lwip/lwip_state.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/*
 * dns_resolve_a — resolve hostname to IPv4 address (host byte order).
 *
 * Delegates to lwIP's DNS resolver via lwip_dns_resolve().
 * Returns 0 on success, -1 on failure or timeout.
 *
 * Status: Compiled. Not yet runtime-tested.
 */
int dns_resolve_a(const char *hostname, u32 *address)
{
    if (!hostname || !address || !*hostname) {
        kprintf("dns: invalid arguments\n");
        return -1;
    }

    if (!lwip_is_ready()) {
        kprintf("dns: lwIP not ready\n");
        return -1;
    }

    kprintf("dns: resolving '%s' via lwIP\n", hostname);
    int rc = lwip_dns_resolve(hostname, address);
    if (rc != 0)
        kprintf("dns: resolution failed for '%s'\n", hostname);
    return rc;
}
