/*
 * Galio Kernel - lwIP TCP compatibility layer
 *
 * Implements Galio's include/net/tcp.h interface using lwIP TCP raw PCBs.
 */

#include "common.h"
#include "net/packet.h"
#include "net/net.h"
#include "net/lwip/lwip_netif.h"
#include "lib/string.h"
#include "lib/kprintf.h"
#include "drivers/pit.h"

#include "lwip/opt.h"
#include "lwip/tcp.h"
#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"

#define TCP_MAX_COMPAT_CONNS 16
#define TCP_COMPAT_RECV_BUF  4096

typedef enum {
    CONN_UNUSED = 0,
    CONN_CONNECTING,
    CONN_ESTABLISHED,
    CONN_LISTENING,
    CONN_CLOSED,
    CONN_ERROR
} conn_state_t;

typedef struct {
    struct tcp_pcb *pcb;
    conn_state_t state;
    u32 remote_ip;
    u16 remote_port;
    u32 last_err;

    /* Pending accepted connections for listening sockets */
    u16 pending[8];
    u8  pending_count;
    u8  pending_head;

    /* Receive buffer */
    u8 recv_buf[TCP_COMPAT_RECV_BUF];
    u32 recv_len;
} lwip_tcp_conn_t;

static lwip_tcp_conn_t conns[TCP_MAX_COMPAT_CONNS];

int galio_tcp_init(void) {
    memset(conns, 0, sizeof(conns));
    return 0;
}

static lwip_tcp_conn_t *alloc_conn(void) {
    for (int i = 0; i < TCP_MAX_COMPAT_CONNS; i++) {
        if (conns[i].state == CONN_UNUSED) {
            memset(&conns[i], 0, sizeof(lwip_tcp_conn_t));
            return &conns[i];
        }
    }
    return NULL;
}

static void tcp_compat_err(void *arg, err_t err) {
    lwip_tcp_conn_t *c = (lwip_tcp_conn_t *)arg;
    if (c) {
        c->pcb = NULL; /* lwIP frees PCB on err callback */
        c->state = CONN_ERROR;
        c->last_err = err;
    }
}

static err_t tcp_compat_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    (void)err;
    lwip_tcp_conn_t *c = (lwip_tcp_conn_t *)arg;
    if (!c) {
        if (p) pbuf_free(p);
        return ERR_OK;
    }

    if (!p) {
        /* Remote closed connection */
        c->state = CONN_CLOSED;
        return ERR_OK;
    }

    u32 space = TCP_COMPAT_RECV_BUF - c->recv_len;
    u32 to_copy = p->tot_len;
    if (to_copy > space) to_copy = space;

    if (to_copy > 0) {
        pbuf_copy_partial(p, c->recv_buf + c->recv_len, (u16_t)to_copy, 0);
        c->recv_len += to_copy;
        tcp_recved(tpcb, (u16_t)to_copy);
    }
    pbuf_free(p);
    return ERR_OK;
}

static err_t tcp_compat_connected(void *arg, struct tcp_pcb *tpcb, err_t err) {
    (void)tpcb;
    lwip_tcp_conn_t *c = (lwip_tcp_conn_t *)arg;
    if (c) {
        if (err == ERR_OK) {
            c->state = CONN_ESTABLISHED;
        } else {
            c->state = CONN_ERROR;
            c->last_err = err;
        }
    }
    return ERR_OK;
}

static err_t tcp_compat_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    lwip_tcp_conn_t *listener = (lwip_tcp_conn_t *)arg;
    if (!listener || err != ERR_OK || !newpcb) {
        return ERR_VAL;
    }

    if (listener->pending_count >= 8) {
        return ERR_MEM;
    }

    lwip_tcp_conn_t *child = alloc_conn();
    if (!child) return ERR_MEM;

    child->pcb = newpcb;
    child->state = CONN_ESTABLISHED;
    child->remote_ip = lwip_ntohl(ip_2_ip4(&newpcb->remote_ip)->addr);
    child->remote_port = newpcb->remote_port;

    tcp_arg(newpcb, child);
    tcp_recv(newpcb, tcp_compat_recv);
    tcp_err(newpcb, tcp_compat_err);

    u16 child_id = (u16)(child - conns) + 1;
    u8 slot = (u8)((listener->pending_head + listener->pending_count) % 8);
    listener->pending[slot] = child_id;
    listener->pending_count++;

    return ERR_OK;
}

int galio_tcp_connect(u32 dest_ip, u16 dest_port) {
    if (!dest_ip || !dest_port) return -1;
    lwip_tcp_conn_t *c = alloc_conn();
    if (!c) return -1;

    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) {
        c->state = CONN_UNUSED;
        return -1;
    }

    c->pcb = pcb;
    c->state = CONN_CONNECTING;
    c->remote_ip = dest_ip;
    c->remote_port = dest_port;

    tcp_arg(pcb, c);
    tcp_recv(pcb, tcp_compat_recv);
    tcp_err(pcb, tcp_compat_err);

    ip_addr_t ip;
    ip4_addr_set_u32(ip_2_ip4(&ip), lwip_htonl(dest_ip));

    err_t err = tcp_connect(pcb, &ip, dest_port, tcp_compat_connected);
    if (err != ERR_OK) {
        tcp_abort(pcb);
        c->state = CONN_UNUSED;
        return -1;
    }

    u32 start = pit_get_ticks();
    /* Poll until connected or timeout (3 seconds = 3000 ticks at 1000 Hz) */
    while (c->state == CONN_CONNECTING && (pit_get_ticks() - start) < 3000) {
        net_poll();
        __asm__ volatile("pause");
    }

    if (c->state != CONN_ESTABLISHED) {
        if (c->pcb) {
            tcp_abort(c->pcb);
            c->pcb = NULL;
        }
        c->state = CONN_UNUSED;
        return -1;
    }

    return (int)(c - conns) + 1;
}

int galio_tcp_listen(u32 local_ip, u16 local_port, u32 backlog) {
    (void)backlog;
    lwip_tcp_conn_t *c = alloc_conn();
    if (!c) return -12;

    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) {
        c->state = CONN_UNUSED;
        return -12;
    }

    ip_addr_t ip;
    ip4_addr_set_u32(ip_2_ip4(&ip), lwip_htonl(local_ip));

    err_t err = tcp_bind(pcb, &ip, local_port);
    if (err != ERR_OK) {
        tcp_abort(pcb);
        c->state = CONN_UNUSED;
        return -22;
    }

    struct tcp_pcb *lpcb = tcp_listen_with_backlog(pcb, 8);
    if (!lpcb) {
        c->state = CONN_UNUSED;
        return -12;
    }

    c->pcb = lpcb;
    c->state = CONN_LISTENING;
    tcp_arg(lpcb, c);
    tcp_accept(lpcb, tcp_compat_accept);

    return (int)(c - conns) + 1;
}

int galio_tcp_accept(u32 listener_id, u32 timeout_ms, u32 *remote_ip, u16 *remote_port) {
    if (listener_id == 0 || listener_id > TCP_MAX_COMPAT_CONNS) return -9;
    lwip_tcp_conn_t *l = &conns[listener_id - 1];
    if (l->state != CONN_LISTENING) return -22;

    u32 start = pit_get_ticks();
    u32 timeout_ticks = timeout_ms; /* PIT at 1000 Hz → 1 ms/tick */
    while (l->pending_count == 0) {
        if (timeout_ms && (pit_get_ticks() - start) >= timeout_ticks) return -11;
        net_poll();
        __asm__ volatile("pause");
    }

    u16 child_id = l->pending[l->pending_head];
    l->pending_head = (l->pending_head + 1) % 8;
    l->pending_count--;

    lwip_tcp_conn_t *child = &conns[child_id - 1];
    if (remote_ip) *remote_ip = child->remote_ip;
    if (remote_port) *remote_port = child->remote_port;

    return (int)child_id;
}

int galio_tcp_send(u32 conn_id, const void *data, u32 length) {
    if (conn_id == 0 || conn_id > TCP_MAX_COMPAT_CONNS || !data) return -1;
    lwip_tcp_conn_t *c = &conns[conn_id - 1];
    if (c->state != CONN_ESTABLISHED || !c->pcb) return -1;
    if (length == 0) return 0;

    err_t err = tcp_write(c->pcb, data, (u16_t)length, TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK) return -1;
    tcp_output(c->pcb);
    return (int)length;
}

int galio_tcp_receive(u32 conn_id, void *buffer, u32 buffer_len, u32 timeout_ms) {
    if (conn_id == 0 || conn_id > TCP_MAX_COMPAT_CONNS || !buffer || buffer_len == 0) return -1;
    lwip_tcp_conn_t *c = &conns[conn_id - 1];
    if (c->state != CONN_ESTABLISHED && c->state != CONN_CLOSED) return -1;

    u32 start = pit_get_ticks();
    u32 timeout_ticks = timeout_ms; /* PIT at 1000 Hz → 1 ms/tick */

    while (c->recv_len == 0 && c->state == CONN_ESTABLISHED) {
        if (timeout_ms && (pit_get_ticks() - start) >= timeout_ticks) break;
        net_poll();
        __asm__ volatile("pause");
    }

    if (c->recv_len == 0) return 0;

    u32 copy_len = c->recv_len;
    if (copy_len > buffer_len) copy_len = buffer_len;

    memcpy(buffer, c->recv_buf, copy_len);
    if (copy_len < c->recv_len) {
        memmove(c->recv_buf, c->recv_buf + copy_len, c->recv_len - copy_len);
    }
    c->recv_len -= copy_len;

    return (int)copy_len;
}

int galio_tcp_close(u32 conn_id) {
    if (conn_id == 0 || conn_id > TCP_MAX_COMPAT_CONNS) return -1;
    lwip_tcp_conn_t *c = &conns[conn_id - 1];
    if (c->state == CONN_UNUSED) return -1;

    if (c->pcb) {
        tcp_arg(c->pcb, NULL);
        tcp_recv(c->pcb, NULL);
        tcp_err(c->pcb, NULL);
        tcp_close(c->pcb);
        c->pcb = NULL;
    }
    c->state = CONN_UNUSED;
    return 0;
}

void galio_tcp_poll(void) {
}
