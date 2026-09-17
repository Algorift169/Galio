#include "button.h"
#include "framebuffer.h"
#include "gui_scale.h"

static button_t help_button;

void panel_help_button_init(int x, int y) {
    button_init(&help_button, "Help", (u32)x, (u32)y, gui_scaled(44u), gui_scaled(19u),
                FB_COLOR(30u, 46u, 60u), FB_COLOR(235u, 242u, 245u));
}

void panel_help_button_draw(void) {
    button_draw(&help_button);
}

u8 panel_help_button_contains(int x, int y) {
    return button_contains(&help_button, x, y);
}

void panel_help_button_click(void) {
}
