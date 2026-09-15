#include "button.h"
#include "framebuffer.h"

static button_t edit_button;

void panel_edit_button_init(int x, int y) {
    button_init(&edit_button, "Edit", (u32)x, (u32)y, 40u, 19u,
                FB_COLOR(30u, 46u, 60u), FB_COLOR(235u, 242u, 245u));
}

void panel_edit_button_draw(void) {
    button_draw(&edit_button);
}

u8 panel_edit_button_contains(int x, int y) {
    return button_contains(&edit_button, x, y);
}

void panel_edit_button_click(void) {
}
