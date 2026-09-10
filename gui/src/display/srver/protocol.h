#ifndef GUI_DISPLAY_PROTOCOL_H
#define GUI_DISPLAY_PROTOCOL_H

#include "common.h"

#define DISPLAY_SERVER_PROTOCOL_NAME "galio.display"
#define DISPLAY_SERVER_PROTOCOL_MESSAGE_MAX 128u
#define DISPLAY_SERVER_PROTOCOL_VERSION 1u

typedef struct {
    u32 version;
    u32 request_id;
    u32 type;
    u32 payload_len;
    u32 client_id;
} display_server_message_header_t;

typedef struct {
    display_server_message_header_t header;
    char payload[DISPLAY_SERVER_PROTOCOL_MESSAGE_MAX];
} display_server_message_t;

u8 display_server_protocol_validate(const display_server_message_t *message);
void display_server_protocol_init(void);
void display_server_protocol_set_version(u32 version);

#endif /* GUI_DISPLAY_PROTOCOL_H */
