#include "button.h"
#include "framebuffer.h"
#include "common.h"

void apps_container_all_app_icon_draw(int x, int y, u32 size, u32 dot_color);

static button_t apps_container_alp_button;

void apps_container_alp_button_init(int x, int y, u32 width, u32 height,
                                    u32 bg_color, u32 icon_color) {
    button_init(&apps_container_alp_button, "", (u32)x, (u32)y,
                width, height, bg_color, icon_color);
    apps_container_alp_button.background = bg_color;
    apps_container_alp_button.text_color = icon_color;
}

void apps_container_alp_button_draw(void) {
    if (!apps_container_alp_button.visible) {
        return;
    }

    fb_fill_rect((u32)apps_container_alp_button.x,
                 (u32)apps_container_alp_button.y,
                 apps_container_alp_button.width,
                 apps_container_alp_button.height,
                 apps_container_alp_button.background);
    fb_draw_rect((u32)apps_container_alp_button.x,
                 (u32)apps_container_alp_button.y,
                 apps_container_alp_button.width,
                 apps_container_alp_button.height,
                 0x00D0D0D0u);

    u32 size = apps_container_alp_button.height > 14u ? 10u : 8u;
    int icon_x = apps_container_alp_button.x + (int)(apps_container_alp_button.width / 2u) - (int)(size / 2u);
    int icon_y = apps_container_alp_button.y + (int)(apps_container_alp_button.height / 2u) - (int)(size / 2u);
    apps_container_all_app_icon_draw(icon_x, icon_y, size, apps_container_alp_button.text_color);
}

u8 apps_container_alp_button_contains(int x, int y) {
    return button_contains(&apps_container_alp_button, x, y);
}

void apps_container_alp_button_click(void) {
}
