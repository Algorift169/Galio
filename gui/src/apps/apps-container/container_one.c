#include "button.h"
#include "framebuffer.h"
#include "common.h"

#define APPS_CONTAINER_MIN_WIDTH 104u
#define APPS_CONTAINER_BUTTON_GAP 8u
#define APPS_CONTAINER_PADDING 12u
#define APPS_CONTAINER_RADIUS 10u

typedef struct {
    int x;
    int y;
    u32 width;
    u32 height;
    u32 app_count;
    u32 app_slot_size;
    u32 background;
    u32 border_color;
    u32 button_color;
    u8 visible;
} apps_container_one_t;

static apps_container_one_t apps_container_one = {
    .x = 0,
    .y = 0,
    .width = APPS_CONTAINER_MIN_WIDTH,
    .height = 26u,
    .app_count = 0u,
    .app_slot_size = 22u,
    .background = 0x001B2333u,
    .border_color = 0x00D0D0D0u,
    .button_color = 0x001F2A38u,
    .visible = 1u,
};

extern void apps_container_all_app_icon_draw(int x, int y, u32 size, u32 dot_color);


static u8 apps_container_one_point_in_round_rect(int px, int py, int x, int y,
                                                u32 w, u32 h, u32 radius) {
    if (px < x || px >= x + (int)w || py < y || py >= y + (int)h) {
        return 0u;
    }

    int r = (int)radius;
    int dx = px - x;
    int dy = py - y;
    int right = x + (int)w - 1;
    int bottom = y + (int)h - 1;

    if (dx < r && dy < r) {
        return ((dx - r) * (dx - r) + (dy - r) * (dy - r)) <= (r * r);
    }
    if (px > right - r && dy < r) {
        int cx = right - r;
        return ((px - cx) * (px - cx) + (dy - r) * (dy - r)) <= (r * r);
    }
    if (dx < r && py > bottom - r) {
        int cy = bottom - r;
        return ((dx - r) * (dx - r) + (py - cy) * (py - cy)) <= (r * r);
    }
    if (px > right - r && py > bottom - r) {
        int cx = right - r;
        int cy = bottom - r;
        return ((px - cx) * (px - cx) + (py - cy) * (py - cy)) <= (r * r);
    }

    return 1u;
}

static void apps_container_one_draw_round_rect(int x, int y, u32 width, u32 height,
                                             u32 fill_color, u32 border_color) {
    int left = x;
    int top = y;
    int right = x + (int)width - 1;
    int bottom = y + (int)height - 1;

    for (int py = top; py <= bottom; py++) {
        for (int px = left; px <= right; px++) {
            if (apps_container_one_point_in_round_rect(px, py, left, top, width, height, APPS_CONTAINER_RADIUS)) {
                fb_put_pixel((u32)px, (u32)py, fill_color);
            }
        }
    }

    for (int py = top; py <= bottom; py++) {
        for (int px = left; px <= right; px++) {
            if (!apps_container_one_point_in_round_rect(px, py, left, top, width, height, APPS_CONTAINER_RADIUS)) {
                continue;
            }

            u8 on_border = 0u;
            if (px == left || px == right || py == top || py == bottom) {
                on_border = 1u;
            } else {
                int border_margin = 1;
                if ((px <= left + border_margin || px >= right - border_margin ||
                     py <= top + border_margin || py >= bottom - border_margin) &&
                    apps_container_one_point_in_round_rect(px, py, left, top, width, height, APPS_CONTAINER_RADIUS)) {
                    on_border = 1u;
                }
            }

            if (on_border) {
                fb_put_pixel((u32)px, (u32)py, border_color);
            }
        }
    }
}

static void apps_container_one_draw_all_apps_button(void) {
    int button_x = apps_container_one.x + (int)APPS_CONTAINER_PADDING;
    int button_y = apps_container_one.y + 2;
    u32 button_w = 22u;
    u32 button_h = apps_container_one.height - 4u;

    fb_fill_rect((u32)button_x, (u32)button_y, button_w, button_h,
                 apps_container_one.button_color);
    fb_draw_rect((u32)button_x, (u32)button_y, button_w, button_h,
                 apps_container_one.border_color);

    apps_container_all_app_icon_draw(button_x + 4, button_y + 5, 10u,
                                    0x00FFFFFFu);
}

void apps_container_one_init(int x, int y) {
    apps_container_one.x = x;
    apps_container_one.y = y;
    apps_container_one.width = APPS_CONTAINER_MIN_WIDTH;
    apps_container_one.height = 26u;
    apps_container_one.app_count = 0u;
    apps_container_one.visible = 1u;
}

void apps_container_one_set_app_count(u32 app_count) {
    apps_container_one.app_count = app_count;

    u32 total_slots = app_count > 0u ? app_count + 1u : 1u;
    u32 base_width = APPS_CONTAINER_MIN_WIDTH;
    u32 extra_width = (total_slots - 1u) * (apps_container_one.app_slot_size + APPS_CONTAINER_BUTTON_GAP);
    apps_container_one.width = base_width + extra_width;
    if (apps_container_one.width < APPS_CONTAINER_MIN_WIDTH) {
        apps_container_one.width = APPS_CONTAINER_MIN_WIDTH;
    }

    apps_container_one.x = (1024 - (int)apps_container_one.width) / 2;
}

void apps_container_one_draw(void) {
    if (!apps_container_one.visible) {
        return;
    }

    apps_container_one_draw_round_rect((int)apps_container_one.x,
                                      (int)apps_container_one.y,
                                      apps_container_one.width,
                                      apps_container_one.height,
                                      apps_container_one.background,
                                      apps_container_one.border_color);

    apps_container_one_draw_all_apps_button();
}

u8 apps_container_one_contains(int x, int y) {
    if (!apps_container_one.visible) {
        return 0u;
    }
    return (x >= apps_container_one.x && x < (int)(apps_container_one.x + (int)apps_container_one.width) &&
            y >= apps_container_one.y && y < (int)(apps_container_one.y + (int)apps_container_one.height));
}

u32 apps_container_one_get_width(void) {
    return apps_container_one.width;
}

u32 apps_container_one_get_height(void) {
    return apps_container_one.height;
}

void apps_container_one_set_visible(u8 visible) {
    apps_container_one.visible = visible ? 1u : 0u;
}
