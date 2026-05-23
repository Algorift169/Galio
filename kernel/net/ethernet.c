#include "net/ethernet.h"
#include "net/arp.h"
#include "net/ipv4.h"
#include "lib/kprintf.h"

void ethernet_input(net_buf_t *buf) {
    if (!buf || buf->len < sizeof(struct eth_hdr)) return;

    struct eth_hdr *eth = (struct eth_hdr *)buf->data;
    u16 ethertype = net_ntohs(eth->type);

    switch (ethertype) {
    case ETH_P_ARP:
        arp_input(buf);
        break;
    case ETH_P_IP:
        ipv4_input(buf);
        break;
    default:
        break;
    }
}
