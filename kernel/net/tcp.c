/*
 * Galio Kernel
 *
 * Copyright (C) 2026 S.M Israfil
 *
 * This file is part of Galio.
 *
 * Galio is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * Galio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Galio. If not, see <https://www.gnu.org/licenses/>.
 */

#include "net/tcp.h"
#include "net/ipv4.h"
#include "net/udp.h"
#include "net/netdev.h"
#include "net/arp.h"
#include "net/net.h"
#include "net/ethernet.h"
#include "net/packet.h"
#include "process/process.h"
#include "drivers/pit.h"
#include "lib/kprintf.h"
#include "lib/string.h"

#define TCP_MAX_CONNECTIONS 64
#define TCP_HASH_BUCKETS 64u
#define TCP_RECV_BUFFER_SIZE 8192
#define TCP_CONNECT_TIMEOUT_TICKS 3000
#define TCP_RETRANSMIT_TICKS 1000
#define TCP_MAX_RETRIES 3
#define TCP_LISTEN_BACKLOG 8u

typedef enum {
    TCP_STATE_CLOSED = 0,
    TCP_STATE_SYN_SENT,
    TCP_STATE_SYN_RECEIVED,
    TCP_STATE_ESTABLISHED,
    TCP_STATE_FIN_WAIT,
} tcp_state_t;

struct tcp_hdr {
    u16 src_port;
    u16 dest_port;
    u32 seq;
    u32 ack;
    u8 data_offset_reserved;
    u8 flags;
    u16 window;
    u16 checksum;
    u16 urgent_ptr;
} __attribute__((packed));

typedef struct {
    u8 used;
    tcp_state_t state;
    u32 src_ip;
    u32 dest_ip;
    u16 src_port;
    u16 dest_port;
    u32 seq;
    u32 ack;
    u32 remote_seq;
    u32 last_activity;
    u8 retries;
    u8 hash_bucket;
    u8 hash_linked;
    u16 hash_next;
    u8 listener;
    u8 parent_listener;
    u8 pending_count;
    u8 pending_head;
    u16 pending[TCP_LISTEN_BACKLOG];
    process_wait_queue_t accept_waiters;
    process_wait_queue_t recv_waiters;
    u32 recv_len;
    u8 recv_buffer[TCP_RECV_BUFFER_SIZE];
} tcp_connection_t;

static tcp_connection_t tcp_connections[TCP_MAX_CONNECTIONS];
static u16 tcp_hash_heads[TCP_HASH_BUCKETS];
static u16 tcp_ephemeral_port = 30000;

static u32 tcp_tuple_hash(u32 src_ip, u16 src_port, u32 dest_ip,
                          u16 dest_port) {
    u32 hash = src_ip ^ dest_ip ^ ((u32)src_port << 16) ^ dest_port;
    hash ^= hash >> 16;
    return hash & (TCP_HASH_BUCKETS - 1u);
}

static void tcp_hash_insert(tcp_connection_t *connection) {
    u32 bucket;
    u32 index;
    if (!connection || connection->hash_linked) return;
    index = (u32)(connection - tcp_connections);
    bucket = tcp_tuple_hash(connection->src_ip, connection->src_port,
                            connection->dest_ip, connection->dest_port);
    connection->hash_bucket = (u8)bucket;
    connection->hash_next = tcp_hash_heads[bucket];
    tcp_hash_heads[bucket] = (u16)index;
    connection->hash_linked = 1u;
}

static void tcp_hash_remove(tcp_connection_t *connection) {
    u32 bucket;
    u16 *link;
    u16 index;
    if (!connection || !connection->hash_linked) return;
    bucket = connection->hash_bucket;
    index = (u16)(connection - tcp_connections);
    link = &tcp_hash_heads[bucket];
    while (*link != TCP_MAX_CONNECTIONS) {
        if (*link == index) {
            *link = connection->hash_next;
            connection->hash_linked = 0u;
            return;
        }
        link = &tcp_connections[*link].hash_next;
    }
    connection->hash_linked = 0u;
}

static u16 tcp_checksum(const struct ipv4_hdr *ip, const struct tcp_hdr *tcp, const void *data, u32 len) {
    u32 sum = 0;
    u32 src = net_ntohl(ip->src);
    u32 dest = net_ntohl(ip->dest);
    sum += (src >> 16) & 0xFFFFu;
    sum += src & 0xFFFFu;
    sum += (dest >> 16) & 0xFFFFu;
    sum += dest & 0xFFFFu;
    sum += IPV4_PROTO_TCP;
    sum += (u16)(sizeof(struct tcp_hdr) + len);

    const u16 *words = (const u16 *)tcp;
    for (u32 i = 0; i + 1 < sizeof(struct tcp_hdr); i += 2) {
        sum += net_ntohs(words[i / 2]);
        if (sum > 0xFFFFu) sum = (sum & 0xFFFFu) + (sum >> 16);
    }
    const u8 *payload = (const u8 *)data;
    for (u32 i = 0; i + 1 < len; i += 2) {
        sum += (payload[i] << 8) | payload[i + 1];
        if (sum > 0xFFFFu) sum = (sum & 0xFFFFu) + (sum >> 16);
    }
    if (len & 1) {
        sum += payload[len - 1] << 8;
        if (sum > 0xFFFFu) sum = (sum & 0xFFFFu) + (sum >> 16);
    }
    while (sum >> 16) sum = (sum & 0xFFFFu) + (sum >> 16);
    return (u16)~sum;
}

static tcp_connection_t *tcp_alloc(void) {
    for (u32 i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        if (!tcp_connections[i].used) {
            memset(&tcp_connections[i], 0, sizeof(tcp_connection_t));
            tcp_connections[i].used = 1;
            tcp_connections[i].hash_next = TCP_MAX_CONNECTIONS;
            tcp_connections[i].state = TCP_STATE_CLOSED;
            process_wait_queue_init(&tcp_connections[i].accept_waiters);
            process_wait_queue_init(&tcp_connections[i].recv_waiters);
            return &tcp_connections[i];
        }
    }
    return NULL;
}

static tcp_connection_t *tcp_lookup(u32 src_ip, u16 src_port, u32 dest_ip, u16 dest_port) {
    u32 bucket = tcp_tuple_hash(dest_ip, dest_port, src_ip, src_port);
    u16 index = tcp_hash_heads[bucket];
    while (index != TCP_MAX_CONNECTIONS) {
        tcp_connection_t *connection = &tcp_connections[index];
        if (connection->used && connection->src_ip == dest_ip &&
            connection->dest_ip == src_ip && connection->src_port == dest_port &&
            connection->dest_port == src_port) return connection;
        index = connection->hash_next;
    }
    return NULL;
}

static tcp_connection_t *tcp_find_listener(u32 destination, u16 port) {
    for (u32 index = 0; index < TCP_MAX_CONNECTIONS; index++) {
        tcp_connection_t *connection = &tcp_connections[index];
        if (connection->used && connection->listener &&
            connection->src_port == port &&
            (!connection->src_ip || connection->src_ip == destination))
            return connection;
    }
    return NULL;
}

static int tcp_send_segment(tcp_connection_t *conn, const void *payload, u32 payload_len, u8 flags) {
    if (!conn || !conn->used) return -1;
    net_device_t *dev = netdev_route(conn->dest_ip);
    if (!dev || dev->ip_addr == 0) return -1;

    u32 next_hop = conn->dest_ip;
    if (dev->netmask && ((conn->dest_ip & dev->netmask) != (dev->ip_addr & dev->netmask))) {
        if (!dev->gateway) return -1;
        next_hop = dev->gateway;
    }

    uint8_t dest_mac[ETH_ALEN];
    if (arp_resolve(dev, next_hop, dest_mac) != 0) {
        u32 start = pit_get_ticks();
        while ((pit_get_ticks() - start) < TCP_RETRANSMIT_TICKS) {
            if (arp_resolve_async(dev, next_hop, dest_mac) == 0) break;
        }
        if (arp_resolve_async(dev, next_hop, dest_mac) != 0) return -1;
    }

    u32 packet_len = ETH_HDR_LEN + sizeof(struct ipv4_hdr) + sizeof(struct tcp_hdr) + payload_len;
    net_buf_t *buf = net_buf_alloc(packet_len, 0);
    if (!buf) return -1;
    buf->dev = dev;
    buf->len = packet_len;

    struct eth_hdr *eth = (struct eth_hdr *)buf->data;
    memcpy(eth->dest, dest_mac, ETH_ALEN);
    memcpy(eth->src, dev->mac, ETH_ALEN);
    eth->type = net_htons(ETH_P_IP);

    struct ipv4_hdr *ip = (struct ipv4_hdr *)(buf->data + ETH_HDR_LEN);
    ip->version_ihl = (4 << 4) | (sizeof(struct ipv4_hdr) / 4);
    ip->tos = 0;
    ip->tot_len = net_htons(sizeof(struct ipv4_hdr) + sizeof(struct tcp_hdr) + payload_len);
    ip->id = net_htons(0);
    ip->frag_off = net_htons(0x4000);
    ip->ttl = 64;
    ip->protocol = IPV4_PROTO_TCP;
    ip->checksum = 0;
    ip->src = net_htonl(dev->ip_addr);
    ip->dest = net_htonl(conn->dest_ip);
    ip->checksum = net_htons(ipv4_checksum(ip, sizeof(struct ipv4_hdr)));

    struct tcp_hdr *tcp = (struct tcp_hdr *)(buf->data + ETH_HDR_LEN + sizeof(struct ipv4_hdr));
    tcp->src_port = net_htons(conn->src_port);
    tcp->dest_port = net_htons(conn->dest_port);
    tcp->seq = net_htonl(conn->seq);
    tcp->ack = net_htonl(conn->remote_seq);
    tcp->data_offset_reserved = (sizeof(struct tcp_hdr) / 4) << 4;
    tcp->flags = flags;
    tcp->window = net_htons(4096);
    tcp->checksum = 0;
    tcp->urgent_ptr = 0;

    if (payload_len > 0) {
        memcpy(buf->data + ETH_HDR_LEN + sizeof(struct ipv4_hdr) + sizeof(struct tcp_hdr), payload, payload_len);
    }

    tcp->checksum = net_htons(tcp_checksum(ip, tcp, payload ? payload : "", payload_len));
    int rc = netdev_send_skb(dev, buf);
    net_buf_free(buf);
    if (rc == 0) {
        conn->last_activity = pit_get_ticks();
        if (flags & TCP_FLAG_SYN) {
            conn->seq++;
        }
        if (flags & TCP_FLAG_FIN) {
            conn->seq++;
        }
        if (payload_len > 0) {
            conn->seq += payload_len;
        }
    }
    return rc;
}

int tcp_init(void) {
    memset(tcp_connections, 0, sizeof(tcp_connections));
    for (u32 i = 0; i < TCP_HASH_BUCKETS; i++) tcp_hash_heads[i] = TCP_MAX_CONNECTIONS;
    tcp_ephemeral_port = 30000;
    return 0;
}

int tcp_connect(u32 dest_ip, u16 dest_port) {
    if (dest_ip == 0 || dest_port == 0) return -1;
    tcp_connection_t *conn = tcp_alloc();
    if (!conn) return -1;

    net_device_t *device = netdev_route(dest_ip);
    if (!device || !device->ip_addr) {
        conn->used = 0;
        return -1;
    }
    conn->src_ip = device->ip_addr;
    conn->dest_ip = dest_ip;
    conn->dest_port = dest_port;
    conn->src_port = tcp_ephemeral_port++;
    if (tcp_ephemeral_port == 0) tcp_ephemeral_port = 30000;
    conn->seq = 0x1000u + pit_get_ticks();
    conn->remote_seq = 0;
    conn->state = TCP_STATE_SYN_SENT;
    conn->last_activity = pit_get_ticks();
    conn->retries = 0;
    tcp_hash_insert(conn);

    if (tcp_send_segment(conn, NULL, 0, TCP_FLAG_SYN) != 0) {
        tcp_hash_remove(conn);
        conn->used = 0;
        return -1;
    }

    u32 start = pit_get_ticks();
    while (conn->state == TCP_STATE_SYN_SENT && (pit_get_ticks() - start) < TCP_CONNECT_TIMEOUT_TICKS) {
        net_poll();
    }

    if (conn->state != TCP_STATE_ESTABLISHED) {
        tcp_hash_remove(conn);
        conn->used = 0;
        return -1;
    }
    return (int)(conn - tcp_connections) + 1;
}

int tcp_listen(u32 local_ip, u16 local_port, u32 backlog) {
    tcp_connection_t *listener;
    if (!local_port || backlog == 0u) return -22;
    listener = tcp_alloc();
    if (!listener) return -12;
    listener->listener = 1u;
    listener->state = TCP_STATE_CLOSED;
    listener->src_ip = local_ip;
    listener->src_port = local_port;
    listener->dest_ip = 0u;
    listener->dest_port = 0u;
    listener->pending_count = 0u;
    listener->pending_head = 0u;
    return (int)(listener - tcp_connections) + 1;
}

int tcp_accept(u32 listener_id, u32 timeout_ms, u32 *remote_ip, u16 *remote_port) {
    u32 start = pit_get_ticks();
    if (listener_id == 0u || listener_id > TCP_MAX_CONNECTIONS) return -9;
    tcp_connection_t *listener = &tcp_connections[listener_id - 1u];
    if (!listener->used || !listener->listener) return -22;
    while (!listener->pending_count) {
        if (!timeout_ms || pit_get_ticks() - start >= timeout_ms) return -11;
        process_wait_queue_sleep_until(&listener->accept_waiters,
                                       start + timeout_ms);
    }
    u16 child_index = listener->pending[listener->pending_head];
    listener->pending_head = (listener->pending_head + 1u) % TCP_LISTEN_BACKLOG;
    listener->pending_count--;
    process_wait_queue_wake_one(&listener->accept_waiters);
    tcp_connection_t *child = &tcp_connections[child_index];
    if (remote_ip) *remote_ip = child->dest_ip;
    if (remote_port) *remote_port = child->dest_port;
    return (int)child_index + 1;
}

int tcp_send(u32 conn_id, const void *data, u32 length) {
    if (conn_id == 0 || conn_id > TCP_MAX_CONNECTIONS) return -1;
    tcp_connection_t *conn = &tcp_connections[conn_id - 1];
    if (!conn->used || conn->state != TCP_STATE_ESTABLISHED) return -1;
    if (length == 0) return 0;
    return tcp_send_segment(conn, data, length, TCP_FLAG_PSH | TCP_FLAG_ACK);
}

int tcp_receive(u32 conn_id, void *buffer, u32 buffer_len, u32 timeout_ms) {
    if (conn_id == 0 || conn_id > TCP_MAX_CONNECTIONS || !buffer || buffer_len == 0) return -1;
    tcp_connection_t *conn = &tcp_connections[conn_id - 1];
    if (!conn->used || conn->state != TCP_STATE_ESTABLISHED) return -1;

    u32 start = pit_get_ticks();
    while (conn->recv_len == 0 && (pit_get_ticks() - start) < timeout_ms) {
        process_wait_queue_sleep_until(&conn->recv_waiters,
                                       start + timeout_ms);
    }
    if (conn->recv_len == 0) return 0;

    u32 copy_len = conn->recv_len;
    if (copy_len > buffer_len) copy_len = buffer_len;
    memcpy(buffer, conn->recv_buffer, copy_len);
    if (copy_len < conn->recv_len) {
        u32 remain = conn->recv_len - copy_len;
        for (u32 i = 0; i < remain; i++) {
            conn->recv_buffer[i] = conn->recv_buffer[i + copy_len];
        }
    }
    conn->recv_len -= copy_len;
    return (int)copy_len;
}

int tcp_close(u32 conn_id) {
    if (conn_id == 0 || conn_id > TCP_MAX_CONNECTIONS) return -1;
    tcp_connection_t *conn = &tcp_connections[conn_id - 1];
    if (!conn->used) return -1;
    if (conn->state == TCP_STATE_ESTABLISHED) {
        tcp_send_segment(conn, NULL, 0, TCP_FLAG_FIN | TCP_FLAG_ACK);
    }
    process_wait_queue_wake_all(&conn->accept_waiters);
    process_wait_queue_wake_all(&conn->recv_waiters);
    tcp_hash_remove(conn);
    conn->used = 0;
    return 0;
}

void tcp_input(net_buf_t *buf, struct ipv4_hdr *ip) {
    if (!buf || !ip || buf->len < ETH_HDR_LEN + sizeof(struct ipv4_hdr) + sizeof(struct tcp_hdr)) return;

    u32 ihl = (ip->version_ihl & 0x0F) * 4;
    if (ihl < sizeof(struct ipv4_hdr)) return;

    struct tcp_hdr *tcp = (struct tcp_hdr *)((uint8_t *)ip + ihl);
    u32 total_len = net_ntohs(ip->tot_len);
    if (total_len < ihl + sizeof(struct tcp_hdr)) return;
    u32 tcp_hdr_len = ((tcp->data_offset_reserved >> 4) & 0x0F) * 4;
    if (tcp_hdr_len < sizeof(struct tcp_hdr)) return;
    if (buf->len < ETH_HDR_LEN + ihl + tcp_hdr_len) return;

    u32 packet_data_len = total_len - ihl - tcp_hdr_len;
    const uint8_t *payload = (uint8_t *)tcp + tcp_hdr_len;
    tcp_connection_t *conn = tcp_lookup(net_ntohl(ip->src), net_ntohs(tcp->src_port), net_ntohl(ip->dest), net_ntohs(tcp->dest_port));
    if (!conn && (tcp->flags & TCP_FLAG_SYN) && !(tcp->flags & TCP_FLAG_ACK)) {
        tcp_connection_t *listener = tcp_find_listener(net_ntohl(ip->dest),
                                                       net_ntohs(tcp->dest_port));
        if (listener && listener->pending_count < TCP_LISTEN_BACKLOG) {
            conn = tcp_alloc();
            if (conn) {
                conn->parent_listener = (u8)(listener - tcp_connections);
                conn->state = TCP_STATE_SYN_SENT;
                conn->src_ip = net_ntohl(ip->dest);
                conn->dest_ip = net_ntohl(ip->src);
                conn->src_port = net_ntohs(tcp->dest_port);
                conn->dest_port = net_ntohs(tcp->src_port);
                conn->seq = 0x1000u + pit_get_ticks();
                conn->remote_seq = net_ntohl(tcp->seq) + 1u;
                tcp_hash_insert(conn);
                (void)tcp_send_segment(conn, NULL, 0, TCP_FLAG_SYN | TCP_FLAG_ACK);
                conn->state = TCP_STATE_SYN_RECEIVED;
            }
        }
        return;
    }
    if (!conn) return;

    u16 expected = tcp_checksum(ip, tcp, payload, packet_data_len);
    if (tcp->checksum != net_htons(expected)) return;

    u32 incoming_seq = net_ntohl(tcp->seq);
    u8 flags = tcp->flags;

    conn->last_activity = pit_get_ticks();

    if (conn->state == TCP_STATE_SYN_SENT && (flags & TCP_FLAG_SYN) && (flags & TCP_FLAG_ACK)) {
        conn->remote_seq = incoming_seq + 1;
        conn->ack = conn->remote_seq;
        conn->seq += 1;
        conn->state = TCP_STATE_ESTABLISHED;
        tcp_send_segment(conn, NULL, 0, TCP_FLAG_ACK);
        return;
    }

    if (conn->state == TCP_STATE_SYN_RECEIVED && (flags & TCP_FLAG_ACK) &&
        net_ntohl(tcp->ack) == conn->seq) {
        conn->state = TCP_STATE_ESTABLISHED;
        tcp_connection_t *listener = &tcp_connections[conn->parent_listener];
        if (listener->used && listener->listener &&
            listener->pending_count < TCP_LISTEN_BACKLOG) {
            u32 tail = (listener->pending_head + listener->pending_count) % TCP_LISTEN_BACKLOG;
            listener->pending[tail] = (u16)(conn - tcp_connections);
            listener->pending_count++;
            process_wait_queue_wake_one(&listener->accept_waiters);
        }
        return;
    }

    if (conn->state == TCP_STATE_ESTABLISHED) {
        if (packet_data_len > 0 && incoming_seq == conn->remote_seq) {
            u32 copy_len = packet_data_len;
            if (copy_len > TCP_RECV_BUFFER_SIZE - conn->recv_len) {
                copy_len = TCP_RECV_BUFFER_SIZE - conn->recv_len;
            }
            if (copy_len > 0) {
                memcpy(conn->recv_buffer + conn->recv_len, payload, copy_len);
                conn->recv_len += copy_len;
                conn->remote_seq += packet_data_len;
                tcp_send_segment(conn, NULL, 0, TCP_FLAG_ACK);
                process_wait_queue_wake_all(&conn->recv_waiters);
            }
        }
        if ((flags & TCP_FLAG_FIN) && incoming_seq == conn->remote_seq) {
            conn->remote_seq++;
            tcp_send_segment(conn, NULL, 0, TCP_FLAG_ACK);
            conn->state = TCP_STATE_FIN_WAIT;
            process_wait_queue_wake_all(&conn->recv_waiters);
        }
    }
}

void tcp_poll(void) {
    u32 now = pit_get_ticks();
    for (u32 i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        tcp_connection_t *conn = &tcp_connections[i];
        if (!conn->used || conn->state != TCP_STATE_SYN_SENT) continue;
        if (now - conn->last_activity >= TCP_RETRANSMIT_TICKS) {
            if (conn->retries++ >= TCP_MAX_RETRIES) {
                tcp_hash_remove(conn);
                conn->used = 0;
            } else {
                tcp_send_segment(conn, NULL, 0, TCP_FLAG_SYN);
                conn->last_activity = now;
            }
        }
    }
}
