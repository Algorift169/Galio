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

#include "net/http.h"
#include "net/tcp.h"
#include "lib/string.h"
#include "lib/kprintf.h"
#include <string.h>

int http_init(void) {
    return 0;
}

static char *append_str(char *dst, const char *src) {
    while (*src) {
        *dst++ = *src++;
    }
    return dst;
}

static char *append_decimal(char *dst, u32 value) {
    char temp[12];
    int len = 0;
    if (value == 0) {
        temp[len++] = '0';
    } else {
        while (value > 0 && len < (int)sizeof(temp)) {
            temp[len++] = (char)('0' + (value % 10));
            value /= 10;
        }
    }
    for (int i = len - 1; i >= 0; i--) {
        *dst++ = temp[i];
    }
    return dst;
}

int http_get_ip(u32 server_ip, u16 server_port, const char *path, char *response, u32 max_response, u32 timeout_ms) {
    if (!path || !response || max_response == 0 || server_ip == 0 || server_port == 0) return -1;

    char effective_path[256];
    if (*path == '\0') {
        strcpy(effective_path, "/");
    } else if (*path != '/') {
        effective_path[0] = '/';
        u32 len = strlen(path);
        if (len >= sizeof(effective_path)) return -1;
        memcpy(effective_path + 1, path, len + 1);
    } else {
        if (strlen(path) >= sizeof(effective_path)) return -1;
        strcpy(effective_path, path);
    }

    int conn = tcp_connect(server_ip, server_port);
    if (conn < 0) return -1;

    char request[512];
    char *p = request;
    p = append_str(p, "GET ");
    p = append_str(p, effective_path);
    p = append_str(p, " HTTP/1.0\r\nHost: ");
    p = append_decimal(p, (server_ip >> 24) & 0xFF);
    *p++ = '.';
    p = append_decimal(p, (server_ip >> 16) & 0xFF);
    *p++ = '.';
    p = append_decimal(p, (server_ip >> 8) & 0xFF);
    *p++ = '.';
    p = append_decimal(p, server_ip & 0xFF);
    p = append_str(p, "\r\nConnection: close\r\n\r\n");

    u32 req_len = (u32)(p - request);
    if (req_len >= sizeof(request)) {
        tcp_close(conn);
        return -1;
    }

    if (tcp_send((u32)conn, request, req_len) < 0) {
        tcp_close(conn);
        return -1;
    }

    u32 total = 0;
    while (total + 1 < max_response) {
        int got = tcp_receive((u32)conn, response + total, max_response - total - 1, timeout_ms);
        if (got <= 0) break;
        total += (u32)got;
    }
    response[total < max_response ? total : max_response - 1] = '\0';
    tcp_close(conn);
    return (int)total;
}
