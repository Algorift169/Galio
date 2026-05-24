#ifndef INCLUDE_NET_TCP_H
#define INCLUDE_NET_TCP_H

#include "common.h"
#include "net/packet.h"
#include "net/netdev.h"
#include "net/ipv4.h"

#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10

int tcp_init(void);
int tcp_connect(u32 dest_ip, u16 dest_port);
int tcp_send(u32 conn_id, const void *data, u32 length);
int tcp_receive(u32 conn_id, void *buffer, u32 buffer_len, u32 timeout_ms);
int tcp_close(u32 conn_id);
void tcp_input(net_buf_t *buf, struct ipv4_hdr *ip);
void tcp_poll(void);

#endif /* INCLUDE_NET_TCP_H */
