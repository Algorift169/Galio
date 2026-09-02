#include "ip.h"
#include "kprintf.h"
#include "string.h"
#include "net/netdev.h"
#include "net/dhcp.h"
#include "net/route.h"

static void ip_print(u32 value) {
    kprintf("%u.%u.%u.%u", (value >> 24) & 255, (value >> 16) & 255,
            (value >> 8) & 255, value & 255);
}

static void mac_print(const u8 *mac) {
    kprintf("%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2],
            mac[3], mac[4], mac[5]);
}

static net_device_t *selected_device(const char *args) {
    char name[NET_NAME_LEN];
    u32 i = 0;
    while (*args == ' ') args++;
    while (*args && *args != ' ' && i + 1 < sizeof(name)) name[i++] = *args++;
    name[i] = 0;
    return i ? netdev_get_by_name(name) : netdev_first();
}

u8 shell_ifconfig_command(const char *args, const char *current_dir) {
    (void)current_dir;
    net_device_t *dev = selected_device(args ? args : "");
    if (!dev) {
        kprintf("No network interfaces\n");
        return 0;
    }
    kprintf("%s\n  state: %s, link: %s\n  mac: ", dev->name,
            (dev->flags & NETIF_UP) ? "UP" : "DOWN",
            netdev_get_link(dev) ? "UP" : "DOWN");
    mac_print(dev->mac);
    kprintf("\n  ipv4: ");
    if (dev->ip_addr) ip_print(dev->ip_addr); else kprintf("none");
    kprintf("\n  netmask: ");
    if (dev->netmask) ip_print(dev->netmask); else kprintf("none");
    kprintf("\n  broadcast: ");
    if (dev->broadcast) ip_print(dev->broadcast); else kprintf("none");
    kprintf("\n  gateway: ");
    if (dev->gateway) ip_print(dev->gateway); else kprintf("none");
    kprintf("\n  dhcp: %s\n  lease: %u seconds\n  dns: ",
            dhcp_state_name(dev->dhcp_state), dev->lease_duration);
    if (dev->dns_servers[0]) ip_print(dev->dns_servers[0]); else kprintf("none");
    if (dev->dns_servers[1]) { kprintf(", "); ip_print(dev->dns_servers[1]); }
    kprintf("\n");
    return 1;
}

u8 shell_ip_command(const char *args, const char *current_dir) {
    (void)current_dir;
    if (!args) args = "";
    while (*args == ' ') args++;
    if (strncmp(args, "addr", 4) == 0) return shell_ifconfig_command(args + 4, current_dir);
    if (strncmp(args, "dhcp", 4) == 0 && (args[4] == ' ' || args[4] == '\0')) {
        net_device_t *dev = selected_device(args + 4);
        if (!dev || dhcp_start() != 0) { kprintf("DHCP configuration failed\n"); return 0; }
        kprintf("DHCP configuration acquired on %s\n", dev->name);
        return 1;
    }
    if (strncmp(args, "renew", 5) == 0) {
        net_device_t *dev = selected_device(args + 5);
        if (!dev || dhcp_renew(dev) != 0) { kprintf("DHCP renew failed\n"); return 0; }
        kprintf("DHCP renewal requested for %s\n", dev->name);
        return 1;
    }
    if (strncmp(args, "release", 7) == 0) {
        net_device_t *dev = selected_device(args + 7);
        if (!dev || dhcp_release(dev) != 0) { kprintf("DHCP release failed\n"); return 0; }
        kprintf("DHCP lease released on %s\n", dev->name);
        return 1;
    }
    if (strncmp(args, "route", 5) == 0) { route_print(); return 1; }
    kprintf("Usage: ip <addr|route|renew|release> [interface]\n");
    return 0;
}
