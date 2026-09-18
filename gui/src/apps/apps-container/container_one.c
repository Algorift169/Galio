#include "button.h"
#include "framebuffer.h"
#include "common.h"

#define APPS_CONTAINER_MIN_WIDTH 104u
#define APPS_CONTAINER_HEIGHT 54u
#define APPS_CONTAINER_BUTTON_GAP 8u
#define APPS_CONTAINER_PADDING 12u
#define APPS_CONTAINER_RADIUS 10u
#define APPS_CONTAINER_ALPHA 128u

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
    .height = APPS_CONTAINER_HEIGHT,
    .app_count = 0u,
    .app_slot_size = 22u,
    .background = FB_COLOR(42u, 62u, 78u),
    .border_color = FB_COLOR(208u, 208u, 208u),
    .button_color = FB_COLOR(255u, 255u, 255u),
    .visible = 1u,
};

extern void apps_container_all_app_icon_draw(int x, int y, u32 size, u32 dot_color);
extern void gsh_button_draw_icon(int x, int y, u32 size);


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
                fb_blend_pixel((u32)px, (u32)py, fill_color, APPS_CONTAINER_ALPHA);
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

static void apps_container_one_draw_solid_round_rect(int x, int y, u32 width, u32 height,
                                                     u32 fill_color, u32 border_color,
                                                     u32 radius) {
    int right = x + (int)width - 1;
    int bottom = y + (int)height - 1;

    for (int py = y; py <= bottom; py++) {
        for (int px = x; px <= right; px++) {
            if (apps_container_one_point_in_round_rect(px, py, x, y, width, height, radius)) {
                fb_put_pixel((u32)px, (u32)py, fill_color);
            }
        }
    }
    for (int py = y; py <= bottom; py++) {
        for (int px = x; px <= right; px++) {
            if (!apps_container_one_point_in_round_rect(px, py, x, y, width, height, radius)) {
                continue;
            }
            if (px == x || px == right || py == y || py == bottom) {
                fb_put_pixel((u32)px, (u32)py, border_color);
            }
        }
    }
}

static void apps_container_one_draw_all_apps_button(void) {
    int button_x = apps_container_one.x + (int)APPS_CONTAINER_PADDING;
    u32 button_w = 34u;
    u32 button_h = 34u;
    int button_y = apps_container_one.y + ((int)apps_container_one.height - (int)button_h) / 2;

    apps_container_one_draw_solid_round_rect(button_x, button_y, button_w, button_h,
                                             apps_container_one.button_color,
                                             FB_COLOR(60u, 60u, 60u), 7u);

    apps_container_all_app_icon_draw(button_x + 9, button_y + 9, 16u,
                                    FB_COLOR(40u, 40u, 40u));
}

static void apps_container_one_draw_gsh_button(void) {
    int button_x = apps_container_one.x + (int)APPS_CONTAINER_PADDING +
                   (int)APPS_CONTAINER_BUTTON_GAP + 34;
    u32 button_w = 34u;
    u32 button_h = 34u;
    int button_y = apps_container_one.y + ((int)apps_container_one.height - (int)button_h) / 2;

    apps_container_one_draw_solid_round_rect(button_x, button_y, button_w, button_h,
                                             FB_COLOR(255u, 255u, 255u),
                                             FB_COLOR(255u, 255u, 255u), 7u);
    gsh_button_draw_icon(button_x + 4, button_y + 3, 24u);
}

void apps_container_one_init(int x, int y) {
    apps_container_one.x = x;
    apps_container_one.y = y;
    apps_container_one.width = APPS_CONTAINER_MIN_WIDTH;
    apps_container_one.height = APPS_CONTAINER_HEIGHT;
    apps_container_one.app_count = 0u;
    apps_container_one.visible = 1u;
}

void apps_container_one_set_app_count(u32 app_count) {
    u32 screen_width = FB_DEFAULT_WIDTH;

    fb_get_info(&screen_width, NULL, NULL, NULL);
    if (screen_width == 0u) screen_width = FB_DEFAULT_WIDTH;

    apps_container_one.app_count = app_count;

    u32 total_slots = app_count > 0u ? app_count + 1u : 1u;
    u32 base_width = APPS_CONTAINER_MIN_WIDTH;
    u32 extra_width = (total_slots - 1u) * (apps_container_one.app_slot_size + APPS_CONTAINER_BUTTON_GAP);
    apps_container_one.width = base_width + extra_width;
    if (apps_container_one.width < APPS_CONTAINER_MIN_WIDTH) {
        apps_container_one.width = APPS_CONTAINER_MIN_WIDTH;
    }

    if (screen_width > apps_container_one.width) {
        apps_container_one.x = ((int)screen_width - (int)apps_container_one.width) / 2;
    } else {
        apps_container_one.x = 0;
        apps_container_one.width = screen_width;
    }

}

void apps_container_one_draw(void) {
    if (!apps_container_one.visible) {
        return;
    }

    apps_container_one_draw_round_rect(apps_container_one.x, apps_container_one.y,
                                       apps_container_one.width, apps_container_one.height,
                                       FB_COLOR(255u, 255u, 255u),
                                       apps_container_one.border_color);

    apps_container_one_draw_all_apps_button();
    if (apps_container_one.app_count > 0u) {
        apps_container_one_draw_gsh_button();
    }
}

u8 apps_container_one_contains(int x, int y) {
    if (!apps_container_one.visible) {
        return 0u;
    }
    return (x >= apps_container_one.x && x < (int)(apps_container_one.x + (int)apps_container_one.width) &&
            y >= apps_container_one.y && y < (int)(apps_container_one.y + (int)apps_container_one.height));
}

u8 apps_container_one_contains_gsh(int x, int y) {
    int button_x;
    int button_y;

    if (!apps_container_one.visible || apps_container_one.app_count == 0u) return 0u;
    button_x = apps_container_one.x + (int)APPS_CONTAINER_PADDING +
               (int)APPS_CONTAINER_BUTTON_GAP + 34;
    button_y = apps_container_one.y + ((int)apps_container_one.height - 34) / 2;
    return (u8)(x >= button_x && x < button_x + 34 &&
                y >= button_y && y < button_y + 34);
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
