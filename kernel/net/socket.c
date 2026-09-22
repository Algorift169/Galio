#include "net/socket.h"
#include "net/tcp.h"
#include "net/udp.h"
#include "net/ethernet.h"
#include "net/netdev.h"
#include "mm/heap.h"
#include "lib/string.h"
#include "process/process.h"
#include "vfs.h"

#define SOCKET_ERROR (-38)

typedef struct {
    u8 used;
    u32 owner_pid;
    u8 type;
    u8 connected;
    u8 closing;
    u8 listening;
    u32 tcp_id;
    udp_socket_t *udp;
    u32 remote_ip;
    u16 remote_port;
} socket_entry_t;

static socket_entry_t socket_table[GALIO_SOCKET_MAX];

static socket_entry_t *socket_from_handle(u32 handle) {
    if (!socket_is_handle(handle)) return NULL;
    u32 index = handle & GALIO_SOCKET_ID_MASK;
    if (index >= GALIO_SOCKET_MAX || !socket_table[index].used) return NULL;
    return &socket_table[index];
}

static u32 socket_handle_from_fd(i32 fd) {
    process_t *process = process_current();
    if (!process || fd < 0 || (u32)fd >= PROCESS_MAX_FDS) return VFS_INVALID_FD;
    return process->fd_table[fd];
}

int socket_is_handle(u32 handle) {
    return (handle & GALIO_SOCKET_FD_FLAG) != 0;
}

int socket_create(u32 owner_pid, i32 domain, i32 type, i32 protocol) {
    if (domain != GALIO_AF_INET ||
        (type != GALIO_SOCK_STREAM && type != GALIO_SOCK_DGRAM) ||
        (protocol != 0 && protocol != GALIO_IPPROTO_TCP &&
         !(type == GALIO_SOCK_DGRAM && protocol == 17))) return SOCKET_ERROR;

    for (u32 index = 0; index < GALIO_SOCKET_MAX; index++) {
        if (!socket_table[index].used) {
            memset(&socket_table[index], 0, sizeof(socket_table[index]));
            socket_table[index].used = 1;
            socket_table[index].owner_pid = owner_pid;
            socket_table[index].type = (u8)type;
            if (type == GALIO_SOCK_DGRAM) {
                socket_table[index].udp = udp_socket_create();
                if (!socket_table[index].udp) {
                    memset(&socket_table[index], 0, sizeof(socket_table[index]));
                    return -12;
                }
            }
            return (int)(GALIO_SOCKET_FD_FLAG | index);
        }
    }
    return -12;
}

int socket_connect_fd(u32 handle, const struct galio_sockaddr_in *address) {
    socket_entry_t *entry = socket_from_handle(handle);
    if (!entry || !address || address->sin_family != GALIO_AF_INET || address->sin_port == 0) return -22;
    if (entry->connected) return -106;

    if (entry->type == GALIO_SOCK_DGRAM) {
        if (!entry->udp) return -9;
        if (!udp_socket_is_bound(entry->udp)) {
            int bind_result = udp_socket_bind(entry->udp, 0u, 0u);
            if (bind_result != 0) return bind_result;
        }
        entry->remote_ip = address->sin_addr;
        entry->remote_port = net_ntohs(address->sin_port);
        entry->connected = 1u;
        return 0;
    }

    int connection_id = tcp_connect(address->sin_addr, address->sin_port);
    if (connection_id < 0) return -110;
    entry->tcp_id = (u32)connection_id;
    entry->connected = 1;
    return 0;
}

int socket_bind_fd(u32 handle, const struct galio_sockaddr_in *address) {
    socket_entry_t *entry = socket_from_handle(handle);
    if (!entry || !address || address->sin_family != GALIO_AF_INET ||
        address->sin_port == 0u) return -22;
    if (entry->type == GALIO_SOCK_DGRAM && entry->udp)
        return udp_socket_bind(entry->udp, address->sin_addr,
                               net_ntohs(address->sin_port));
    if (entry->type == GALIO_SOCK_STREAM && !entry->connected) {
        entry->remote_ip = address->sin_addr;
        entry->remote_port = net_ntohs(address->sin_port);
        return 0;
    }
    return -95;
}

int socket_listen_fd(u32 handle, i32 backlog) {
    socket_entry_t *entry = socket_from_handle(handle);
    if (!entry || entry->type != GALIO_SOCK_STREAM || entry->connected ||
        entry->remote_port == 0u || backlog <= 0) return -22;
    int connection = tcp_listen(entry->remote_ip, entry->remote_port,
                                (u32)backlog);
    if (connection < 0) return connection;
    entry->tcp_id = (u32)connection;
    entry->listening = 1u;
    return 0;
}

int socket_accept_fd(u32 handle, u32 timeout_ms, u32 *remote_ip, u16 *remote_port) {
    socket_entry_t *listener = socket_from_handle(handle);
    if (!listener || !listener->listening) return -22;
    int connection = tcp_accept(listener->tcp_id, timeout_ms, remote_ip, remote_port);
    if (connection < 0) return connection;
    for (u32 index = 0; index < GALIO_SOCKET_MAX; index++) {
        socket_entry_t *entry = &socket_table[index];
        if (!entry->used) {
            memset(entry, 0, sizeof(*entry));
            entry->used = 1u;
            entry->owner_pid = process_current() ? process_current()->pid : 0u;
            entry->type = GALIO_SOCK_STREAM;
            entry->connected = 1u;
            entry->tcp_id = (u32)connection;
            return (int)(GALIO_SOCKET_FD_FLAG | index);
        }
    }
    (void)tcp_close((u32)connection);
    return -12;
}

int socket_set_reuseaddr_fd(u32 handle, u8 enabled) {
    socket_entry_t *entry = socket_from_handle(handle);
    if (!entry) return -9;
    if (entry->type != GALIO_SOCK_DGRAM || !entry->udp) return -95;
    return udp_socket_set_reuseaddr(entry->udp, enabled);
}

 i64 socket_send_fd(u32 handle, const void *buffer, u32 length) {
    socket_entry_t *entry = socket_from_handle(handle);
    if (!entry || !entry->connected) return -107;
    if (!buffer && length != 0) return -14;
    if (length == 0) return 0;
    if (entry->type == GALIO_SOCK_DGRAM) {
        int result = udp_socket_sendto(entry->udp, entry->remote_ip,
                                       entry->remote_port, buffer, length);
        return result < 0 ? result : (i64)length;
    }
    int result = tcp_send(entry->tcp_id, buffer, length);
    return result < 0 ? -5 : result;
}

i64 socket_recv_fd(u32 handle, void *buffer, u32 length, u32 timeout_ms) {
    socket_entry_t *entry = socket_from_handle(handle);
    if (!entry || !entry->connected) return -107;
    if (!buffer || length == 0) return -22;
    if (entry->type == GALIO_SOCK_DGRAM) {
        int result = udp_socket_recvfrom(entry->udp, buffer, length, timeout_ms,
                                         NULL, NULL);
        return result;
    }
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
    if (entry->udp) udp_socket_destroy(entry->udp);
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

int syscall_bind(i32 sockfd, const void *address, u32 address_len) {
    if (!address || address_len < sizeof(struct galio_sockaddr_in)) return -22;
    u32 handle = socket_handle_from_fd(sockfd);
    if (handle == VFS_INVALID_FD) return -9;
    return socket_bind_fd(handle, (const struct galio_sockaddr_in *)address);
}

int syscall_listen(i32 sockfd, i32 backlog) {
    u32 handle = socket_handle_from_fd(sockfd);
    if (handle == VFS_INVALID_FD) return -9;
    return socket_listen_fd(handle, backlog);
}

int syscall_accept(i32 sockfd, void *address, u32 *address_len) {
    u32 handle = socket_handle_from_fd(sockfd);
    u32 remote_ip = 0u;
    u16 remote_port = 0u;
    int accepted;
    if (handle == VFS_INVALID_FD) return -9;
    accepted = socket_accept_fd(handle, 3000u, &remote_ip, &remote_port);
    if (accepted < 0) return accepted;
    if (address && address_len && *address_len >= sizeof(struct galio_sockaddr_in)) {
        struct galio_sockaddr_in *remote = (struct galio_sockaddr_in *)address;
        memset(remote, 0, sizeof(*remote));
        remote->sin_family = GALIO_AF_INET;
        remote->sin_port = net_htons(remote_port);
        remote->sin_addr = remote_ip;
        *address_len = sizeof(*remote);
    }
    process_t *process = process_current();
    if (!process) return -12;
    for (u32 fd = 0; fd < PROCESS_MAX_FDS; fd++) {
        if (process->fd_table[fd] == VFS_INVALID_FD) {
            process->fd_table[fd] = (u32)accepted;
            return (int)fd;
        }
    }
    socket_close_fd((u32)accepted);
    return -12;
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
    u32 handle = socket_handle_from_fd(sockfd);
    if (handle == VFS_INVALID_FD || !value || value_len < sizeof(i32)) return -22;
    if (level != GALIO_SOL_SOCKET || option != GALIO_SO_REUSEADDR) return -95;
    return socket_set_reuseaddr_fd(handle, (*(const i32 *)value) != 0);
}

int syscall_getsockopt(i32 sockfd, i32 level, i32 option, void *value, u32 *value_len) {
    (void)sockfd;
    (void)level;
    (void)option;
    (void)value;
    (void)value_len;
    return SOCKET_ERROR;
}
