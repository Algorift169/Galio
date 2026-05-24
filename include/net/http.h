#ifndef INCLUDE_NET_HTTP_H
#define INCLUDE_NET_HTTP_H

#include "common.h"

int http_init(void);
int http_get_ip(u32 server_ip, u16 server_port, const char *path, char *response, u32 max_response, u32 timeout_ms);

#endif /* INCLUDE_NET_HTTP_H */
