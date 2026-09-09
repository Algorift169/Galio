#include "desktop.h"
#include "framebuffer.h"
#include "window.h"
#include "display_wrapper.h"
#include "cursor.h"
#include "gsh_button.h"

static window_t desktop_window;
static u8 gsh_open = 0u;

void desktop_init(void) {
    window_init(&desktop_window, "Desktop", FB_COLOR(30, 60, 90), 0u, 0u, 1024u, 768u);
    desktop_window.draggable = 0u;
    desktop_window.resizeable = 0u;
    desktop_window.visible = 1u;
    desktop_window.closed = 0u;
    gsh_button_init();
}

void desktop_draw(void) {
    display_wrapper_draw();
    gsh_button_draw();
}

void desktop_handle_click(int x, int y) {
    if (x >= 20 && x <= 200 && y >= 680 && y <= 740) {
        gsh_open = 1u;
        gsh_button_click();
    }
}

void desktop_set_background(u32 color) {
    desktop_window.background = color;
}

void desktop_set_gsh_open(u8 open) {
    gsh_open = open;
}

u8 desktop_is_gsh_open(void) {
    return gsh_open;
}

const window_t *desktop_get_window(void) {
    return &desktop_window;
}
