#include "shell.h"
#include "kprintf.h"
#include "user_syscall.h"
#include <string.h>

u8 shell_socket_command(const char *args, const char *current_dir) {
    (void)current_dir;
    
    if (!args || *args == '\0') {
        kprintf("socket: test socket syscalls\n");
        kprintf("usage: socket [test|info]\n");
        return 0;
    }
    
    if (strcmp(args, "test") == 0) {
        /* Test socket syscall - should return ENOSYS */
        int sockfd = sys_socket(2, 1, 6);  /* AF_INET, SOCK_STREAM, IPPROTO_TCP */
        kprintf("socket(AF_INET, SOCK_STREAM, IPPROTO_TCP) = %d\n", sockfd);
        if (sockfd < 0) {
            kprintf("  (Expected: syscall not supported)\n");
        }
        return 1;
    }
    
    if (strcmp(args, "info") == 0) {
        kprintf("Socket syscalls available:\n");
        kprintf("  socket()       - Create socket\n");
        kprintf("  bind()         - Bind socket to address\n");
        kprintf("  listen()       - Listen for connections\n");
        kprintf("  accept()       - Accept connection\n");
        kprintf("  connect()      - Connect to address\n");
        kprintf("  send/recv()    - Send/receive data\n");
        kprintf("  shutdown()     - Shutdown socket\n");
        return 1;
    }
    
    kprintf("socket: unknown subcommand '%s'\n", args);
    return 0;
}
