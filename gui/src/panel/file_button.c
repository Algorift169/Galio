#include "button.h"
#include "framebuffer.h"

static button_t file_button;

void panel_file_button_init(int x, int y) {
    button_init(&file_button, "File", (u32)x, (u32)y, 40u, 19u,
                FB_COLOR(30u, 46u, 60u), FB_COLOR(235u, 242u, 245u));
}

void panel_file_button_draw(void) {
    button_draw(&file_button);
}

u8 panel_file_button_contains(int x, int y) {
    return button_contains(&file_button, x, y);
}

void panel_file_button_click(void) {
}
