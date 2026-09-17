#include "desktop.h"
#include "framebuffer.h"
#include "window.h"
#include "display_wrapper.h"
#include "cursor.h"
#include "gsh_button.h"
#include "panel.h"
#include "srver/client.h"
#include "display_output.h"

extern void apps_container_one_init(int x, int y);
extern void apps_container_one_set_app_count(u32 app_count);
extern void apps_container_one_draw(void);
extern u32 apps_container_one_get_width(void);
extern u32 apps_container_one_get_height(void);

static window_t desktop_window;
static u8 gsh_open = 0u;
static u32 desktop_client_id = 0u;

static int desktop_app_container_y(u32 screen_height) {
    int y = (int)screen_height -
            (int)apps_container_one_get_height() - 16;
    return y < 0 ? 0 : y;
}


static void desktop_init_app_container(void) {
    u32 screen_width = FB_DEFAULT_WIDTH;
    u32 screen_height = FB_DEFAULT_HEIGHT;
    fb_get_info(&screen_width, &screen_height, NULL, NULL);
    screen_height = display_output_get()->usable_height;
    screen_width = display_output_get()->usable_width;

    apps_container_one_init((int)(screen_width / 2u) - 80,
                            desktop_app_container_y(screen_height));
    apps_container_one_set_app_count(0u);
}

void desktop_init(void) {
    u32 screen_width = FB_DEFAULT_WIDTH;
    u32 screen_height = FB_DEFAULT_HEIGHT;

    display_wrapper_init();
    fb_get_info(&screen_width, &screen_height, NULL, NULL);
    if (screen_width == 0u) screen_width = FB_DEFAULT_WIDTH;
    if (screen_height == 0u) screen_height = FB_DEFAULT_HEIGHT;
    screen_height = display_output_get()->usable_height;
    screen_width = display_output_get()->usable_width;

    window_init(&desktop_window, "Desktop", FB_COLOR(30, 60, 90), 0u, 0u, screen_width, screen_height);
    desktop_window.draggable = 0u;
    desktop_window.resizeable = 0u;
    desktop_window.visible = 1u;
    desktop_window.closed = 0u;
    desktop_client_id = display_server_client_connect();
    gsh_button_init();
    panel_init();
    desktop_init_app_container();
}

void desktop_draw(void) {
    display_wrapper_draw();
    panel_draw();
    apps_container_one_draw();
}

void desktop_handle_click(int x, int y) {
    u32 screen_width = FB_DEFAULT_WIDTH;
    u32 screen_height = FB_DEFAULT_HEIGHT;
    u32 dock_width;
    int dock_x;
    int dock_y;

    if (panel_handle_click(x, y)) return;

    fb_get_info(&screen_width, &screen_height, NULL, NULL);
    screen_height = display_output_get()->usable_height;
    dock_width = apps_container_one_get_width();
    dock_x = (int)((screen_width - dock_width) / 2u);
    dock_y = desktop_app_container_y(screen_height);

    if (x >= dock_x && x < dock_x + (int)dock_width &&
        y >= dock_y && y < dock_y + (int)apps_container_one_get_height()) {
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
