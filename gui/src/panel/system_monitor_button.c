#include "button.h"
#include "framebuffer.h"

static button_t system_monitor_button;

void panel_system_monitor_button_init(int x, int y) {
    button_init(&system_monitor_button, "System Monitor", (u32)x, (u32)y,
                100u, 19u, FB_COLOR(30u, 46u, 60u),
                FB_COLOR(105u, 145u, 165u));
}

void panel_system_monitor_button_draw(void) {
    button_draw(&system_monitor_button);
}

u8 panel_system_monitor_button_contains(int x, int y) {
    return button_contains(&system_monitor_button, x, y);
}

void panel_system_monitor_button_click(void) {
}
