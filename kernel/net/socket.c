#include "net/socket.h"
#include "net/tcp.h"
#include "net/netdev.h"
#include "mm/heap.h"
#include "lib/string.h"

#define SOCKET_ERROR (-38)

typedef struct {
    u8 used;
    u32 owner_pid;
    u8 type;
    u8 connected;
    u8 closing;
    u32 tcp_id;
} socket_entry_t;

static socket_entry_t socket_table[GALIO_SOCKET_MAX];

static socket_entry_t *socket_from_handle(u32 handle) {
    if (!socket_is_handle(handle)) return NULL;
    u32 index = handle & GALIO_SOCKET_ID_MASK;
    if (index >= GALIO_SOCKET_MAX || !socket_table[index].used) return NULL;
    return &socket_table[index];
}

int socket_is_handle(u32 handle) {
    return (handle & GALIO_SOCKET_FD_FLAG) != 0;
}

int socket_create(u32 owner_pid, i32 domain, i32 type, i32 protocol) {
    if (domain != GALIO_AF_INET || type != GALIO_SOCK_STREAM ||
        (protocol != 0 && protocol != GALIO_IPPROTO_TCP)) return SOCKET_ERROR;

    for (u32 index = 0; index < GALIO_SOCKET_MAX; index++) {
        if (!socket_table[index].used) {
            memset(&socket_table[index], 0, sizeof(socket_table[index]));
            socket_table[index].used = 1;
            socket_table[index].owner_pid = owner_pid;
            socket_table[index].type = GALIO_SOCK_STREAM;
            return (int)(GALIO_SOCKET_FD_FLAG | index);
        }
    }
    return -12;
}

int socket_connect_fd(u32 handle, const struct galio_sockaddr_in *address) {
    socket_entry_t *entry = socket_from_handle(handle);
    if (!entry || !address || address->sin_family != GALIO_AF_INET || address->sin_port == 0) return -22;
    if (entry->connected) return -106;

    int connection_id = tcp_connect(address->sin_addr, address->sin_port);
    if (connection_id < 0) return -110;
    entry->tcp_id = (u32)connection_id;
    entry->connected = 1;
    return 0;
}

 i64 socket_send_fd(u32 handle, const void *buffer, u32 length) {
    socket_entry_t *entry = socket_from_handle(handle);
    if (!entry || !entry->connected) return -107;
    if (!buffer && length != 0) return -14;
    if (length == 0) return 0;
    int result = tcp_send(entry->tcp_id, buffer, length);
    return result < 0 ? -5 : result;
}

i64 socket_recv_fd(u32 handle, void *buffer, u32 length, u32 timeout_ms) {
    socket_entry_t *entry = socket_from_handle(handle);
    if (!entry || !entry->connected) return -107;
    if (!buffer || length == 0) return -22;
    int result = tcp_receive(entry->tcp_id, buffer, length, timeout_ms);
    return result < 0 ? -5 : result;
}

int socket_shutdown_fd(u32 handle, i32 how) {
    socket_entry_t *entry = socket_from_handle(handle);
    if (!entry || how < 0 || how > 2) return -22;
    if (entry->connected && (how == 1 || how == 2)) {
        tcp_close(entry->tcp_id);
        entry->connected = 0;
    }
    entry->closing = 1;
    return 0;
}

int socket_close_fd(u32 handle) {
    socket_entry_t *entry = socket_from_handle(handle);
    if (!entry) return -9;
    if (entry->connected) tcp_close(entry->tcp_id);
    memset(entry, 0, sizeof(*entry));
    return 0;
}

void socket_process_cleanup(u32 pid, u32 *fd_table, u32 fd_count) {
    if (!fd_table) return;
    for (u32 fd = 0; fd < fd_count; fd++) {
        u32 handle = fd_table[fd];
        socket_entry_t *entry = socket_from_handle(handle);
        if (entry && entry->owner_pid == pid) {
            socket_close_fd(handle);
            fd_table[fd] = 0xFFFFFFFFu;
        }
    }
}

/* These ABI operations require UDP queues or TCP listen/accept support. */
int syscall_accept(i32 sockfd, void *address, u32 *address_len) {
    (void)sockfd;
    (void)address;
    (void)address_len;
    return SOCKET_ERROR;
}

int syscall_bind(i32 sockfd, const void *address, u32 address_len) {
    (void)sockfd;
    (void)address;
    (void)address_len;
    return SOCKET_ERROR;
}

int syscall_listen(i32 sockfd, i32 backlog) {
    (void)sockfd;
    (void)backlog;
    return SOCKET_ERROR;
}

int syscall_sendmsg(i32 sockfd, const void *message, i32 flags) {
    (void)sockfd;
    (void)message;
    (void)flags;
    return SOCKET_ERROR;
}

int syscall_recvmsg(i32 sockfd, void *message, i32 flags) {
    (void)sockfd;
    (void)message;
    (void)flags;
    return SOCKET_ERROR;
}

int syscall_socketpair(i32 domain, i32 type, i32 protocol, i32 *sockets) {
    (void)domain;
    (void)type;
    (void)protocol;
    (void)sockets;
    return SOCKET_ERROR;
}

int syscall_getsockname(i32 sockfd, void *address, u32 *address_len) {
    (void)sockfd;
    (void)address;
    (void)address_len;
    return SOCKET_ERROR;
}

int syscall_getpeername(i32 sockfd, void *address, u32 *address_len) {
    (void)sockfd;
    (void)address;
    (void)address_len;
    return SOCKET_ERROR;
}

int syscall_setsockopt(i32 sockfd, i32 level, i32 option, const void *value, u32 value_len) {
    (void)sockfd;
    (void)level;
    (void)option;
    (void)value;
    (void)value_len;
    return SOCKET_ERROR;
}

int syscall_getsockopt(i32 sockfd, i32 level, i32 option, void *value, u32 *value_len) {
    (void)sockfd;
    (void)level;
    (void)option;
    (void)value;
    (void)value_len;
    return SOCKET_ERROR;
}
