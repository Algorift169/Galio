#include "srver/events.h"

extern display_server_state_t g_display_server_state;

void display_server_events_init(void) {
    u32 index;

    for (index = 0u; index < DISPLAY_SERVER_MAX_EVENTS; index++) {
        g_display_server_state.events[index].type = 0u;
        g_display_server_state.events[index].event_id = 0u;
    }

    g_display_server_state.event_head = 0u;
    g_display_server_state.event_tail = 0u;
}

u8 display_server_events_push(display_server_event_t event) {
    u32 next_index;

    next_index = (g_display_server_state.event_tail + 1u) % DISPLAY_SERVER_MAX_EVENTS;
    if (next_index == g_display_server_state.event_head) {
        return 0u;
    }

    g_display_server_state.events[g_display_server_state.event_tail] = event;
    g_display_server_state.events[g_display_server_state.event_tail].event_id = g_display_server_state.event_tail + 1u;
    g_display_server_state.event_tail = next_index;
    return 1u;
}

display_server_event_t display_server_events_pop(void) {
    display_server_event_t empty_event = {0};

    if (g_display_server_state.event_head == g_display_server_state.event_tail) {
        return empty_event;
    }

    empty_event = g_display_server_state.events[g_display_server_state.event_head];
    g_display_server_state.event_head = (g_display_server_state.event_head + 1u) % DISPLAY_SERVER_MAX_EVENTS;
    return empty_event;
}

u8 display_server_events_pending(void) {
    return g_display_server_state.event_head != g_display_server_state.event_tail;
}
