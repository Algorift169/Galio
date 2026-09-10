#include "srver/protocol.h"

static u32 g_protocol_version = DISPLAY_SERVER_PROTOCOL_VERSION;

void display_server_protocol_init(void) {
    g_protocol_version = DISPLAY_SERVER_PROTOCOL_VERSION;
}

void display_server_protocol_set_version(u32 version) {
    g_protocol_version = version;
}

u8 display_server_protocol_validate(const display_server_message_t *message) {
    if (!message) {
        return 0u;
    }

    if (message->header.version != g_protocol_version) {
        return 0u;
    }

    if (message->header.payload_len > DISPLAY_SERVER_PROTOCOL_MESSAGE_MAX) {
        return 0u;
    }

    return 1u;
}
