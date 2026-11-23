/*
 * Galio Kernel - TCP socket abstraction layer
 *
 * Re-routed to galio_tcp_* implementations powered by lwIP.
 */

#ifndef INCLUDE_NET_TCP_H
#define INCLUDE_NET_TCP_H

#include "common.h"
#include "net/packet.h"
#include "net/netdev.h"

#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10

int galio_tcp_init(void);
int galio_tcp_connect(u32 dest_ip, u16 dest_port);
int galio_tcp_listen(u32 local_ip, u16 local_port, u32 backlog);
int galio_tcp_accept(u32 listener_id, u32 timeout_ms, u32 *remote_ip, u16 *remote_port);
int galio_tcp_send(u32 conn_id, const void *data, u32 length);
int galio_tcp_receive(u32 conn_id, void *buffer, u32 buffer_len, u32 timeout_ms);
int galio_tcp_close(u32 conn_id);
void galio_tcp_poll(void);

#define tcp_init    galio_tcp_init
#define tcp_connect galio_tcp_connect
#define tcp_listen  galio_tcp_listen
#define tcp_accept  galio_tcp_accept
#define tcp_send    galio_tcp_send
#define tcp_receive galio_tcp_receive
#define tcp_close   galio_tcp_close
#define tcp_poll    galio_tcp_poll

#endif /* INCLUDE_NET_TCP_H */
