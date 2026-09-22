#ifndef INCLUDE_NET_SOCKET_H
#define INCLUDE_NET_SOCKET_H

#include "common.h"

#define GALIO_AF_INET 2
#define GALIO_SOCK_STREAM 1
#define GALIO_SOCK_DGRAM 2
#define GALIO_IPPROTO_TCP 6
#define GALIO_SOCKET_FD_FLAG 0x10000000u
#define GALIO_SOCKET_ID_MASK 0x000000FFu
#define GALIO_SOCKET_MAX 16u
#define GALIO_SOL_SOCKET 1
#define GALIO_SO_REUSEADDR 2

struct galio_sockaddr_in {
    u16 sin_family;
    u16 sin_port;
    u32 sin_addr;
    u8 sin_zero[8];
} __attribute__((packed));

int socket_create(u32 owner_pid, i32 domain, i32 type, i32 protocol);
int socket_connect_fd(u32 handle, const struct galio_sockaddr_in *address);
int socket_bind_fd(u32 handle, const struct galio_sockaddr_in *address);
int socket_listen_fd(u32 handle, i32 backlog);
int socket_accept_fd(u32 handle, u32 timeout_ms, u32 *remote_ip, u16 *remote_port);
int socket_set_reuseaddr_fd(u32 handle, u8 enabled);
i64 socket_send_fd(u32 handle, const void *buffer, u32 length);
i64 socket_recv_fd(u32 handle, void *buffer, u32 length, u32 timeout_ms);
int socket_shutdown_fd(u32 handle, i32 how);
int socket_close_fd(u32 handle);
int socket_is_handle(u32 handle);
void socket_process_cleanup(u32 pid, u32 *fd_table, u32 fd_count);

#endif
