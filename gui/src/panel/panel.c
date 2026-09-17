#include "panel.h"
#include "button.h"
#include "clock.h"
#include "drawline.h"
#include "framebuffer.h"
#include "gsh_button.h"
#include "power/power.h"
#include "display_output.h"
#include "gui_scale.h"

#define PANEL_COLOR FB_COLOR(18u, 28u, 42u)
#define PANEL_LINE_COLOR FB_COLOR(105u, 145u, 165u)
#define PANEL_TEXT_COLOR FB_COLOR(235u, 242u, 245u)
#define PANEL_BUTTON_COLOR FB_COLOR(30u, 46u, 60u)
static button_t shutdown_button;
static u8 panel_ready;
static int panel_system_x;
static int panel_file_x;
static int panel_help_x;
static int panel_shutdown_x;

static u32 panel_height(void) { return gui_scaled(36u); }
static u32 panel_gap(void) { return gui_scaled(4u); }
static u32 panel_font_pixel(void) {
    u32 pixel = gui_scaled(1u);
    return pixel == 0u ? 1u : pixel;
}

static const u8 panel_font_digit[10][7] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    {0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F},
    {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E},
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},
    {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E},
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E},
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x1C}
};

static const u8 panel_font_letter[26][7] = {
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}, {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0E}, {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}, {0x07,0x02,0x02,0x02,0x12,0x12,0x0C},
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}, {0x11,0x19,0x1D,0x17,0x13,0x11,0x11},
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}, {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, {0x11,0x11,0x11,0x11,0x0A,0x0A,0x04},
    {0x11,0x11,0x11,0x15,0x15,0x1B,0x11}, {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}, {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}
};

static void panel_put_char(int x, int y, char character, u32 color) {
    const u8 *glyph = NULL;
    u32 pixel = panel_font_pixel();
    if (character >= '0' && character <= '9') glyph = panel_font_digit[character - '0'];
    else if (character >= 'A' && character <= 'Z') glyph = panel_font_letter[character - 'A'];
    if (!glyph) return;
    for (u32 row = 0u; row < 7u; row++) {
        for (u32 column = 0u; column < 5u; column++) {
            if (glyph[row] & (1u << (4u - column))) {
                fb_fill_rect((u32)(x + (int)(column * pixel)),
                             (u32)(y + (int)(row * pixel)), pixel, pixel, color);
            }
        }
    }
}

static void panel_put_text(int x, int y, const char *text, u32 color) {
    u32 advance = panel_font_pixel() * 6u;

    while (text && *text) {
        if (*text != ':' && *text != '-' && *text != '/') panel_put_char(x, y, *text, color);
        x += (int)advance;
        text++;
    }
}

static void panel_put_centered_text(int x, int y, u32 width, u32 height,
                                    const char *text, u32 color) {
    u32 length = 0u;
    u32 text_width;
    u32 pixel = panel_font_pixel();

    while (text && text[length]) length++;
    text_width = length * pixel * 6u;
    panel_put_text(x + (int)(width > text_width ? (width - text_width) / 2u : 0u),
                   y + (int)(height > pixel * 7u ? (height - pixel * 7u) / 2u : 0u),
                   text, color);
}

void panel_draw_clock(void) {
    char date[11];
    char time[9];

    clock_format_datetime(date, time);

    int y = (int)gui_scaled(8u);
    fb_fill_rect(gui_scaled(8u), gui_scaled(8u), gui_scaled(198u), gui_scaled(14u), PANEL_COLOR);
    panel_put_text((int)gui_scaled(10u), y, "DATE", PANEL_LINE_COLOR);
    panel_put_text((int)gui_scaled(55u), y, date, PANEL_TEXT_COLOR);
    panel_put_text((int)gui_scaled(205u), y, "TIME", PANEL_LINE_COLOR);
    panel_put_text((int)gui_scaled(250u), y, time, PANEL_TEXT_COLOR);
}

void panel_init(void) {
    u32 screen_width = FB_DEFAULT_WIDTH;
    u32 screen_height = FB_DEFAULT_HEIGHT;
    int cluster_x;
    int shutdown_x;
    int system_x;
    int file_x;
    int help_x;
    int gsh_x;
    u32 height;

    const display_output_t *output = display_output_get();
    screen_width = output->width;
    screen_height = output->height;
    gui_scale_update(screen_width, screen_height);
    height = panel_height();
    display_output_set_reserved_area(height, 0u);

    cluster_x = (int)screen_width - (int)gui_scaled(300u);
    if (cluster_x < (int)gui_scaled(8u)) cluster_x = (int)gui_scaled(8u);

    system_x = cluster_x;
    file_x = system_x + (int)gui_scaled(100u) + (int)panel_gap();
    gsh_x = file_x + (int)gui_scaled(40u) + (int)panel_gap();
    help_x = gsh_x + (int)gui_scaled(28u) + (int)panel_gap();
    shutdown_x = help_x + (int)gui_scaled(44u) + (int)panel_gap();
    panel_system_x = system_x;
    panel_file_x = file_x;
    panel_help_x = help_x;
    panel_shutdown_x = shutdown_x;

    button_init(&shutdown_button, "SHUTDOWN", (u32)shutdown_x, gui_scaled(6u),
                gui_scaled(62u), gui_scaled(19u),
                PANEL_BUTTON_COLOR, PANEL_TEXT_COLOR);
    panel_system_monitor_button_init(system_x, (int)gui_scaled(6u));
    panel_file_button_init(file_x, (int)gui_scaled(6u));
    panel_help_button_init(help_x, (int)gui_scaled(6u));
    gsh_button_set_position(gsh_x, (int)gui_scaled(6u));
    panel_ready = 1u;
}

void panel_draw(void) {
    u32 screen_width = FB_DEFAULT_WIDTH;
    u32 panel_width;

    if (!panel_ready) return;

    const display_output_t *output = display_output_get();
    screen_width = output->width;
    panel_width = screen_width;
    fb_fill_rect(0u, 0u, panel_width, panel_height(), PANEL_COLOR);
    gui_draw_border(0, 0, panel_width, panel_height(), PANEL_LINE_COLOR);
    panel_draw_clock();
    panel_system_monitor_button_draw();
    panel_file_button_draw();
    panel_help_button_draw();
    button_draw(&shutdown_button);
    panel_put_centered_text(panel_system_x, (int)gui_scaled(6u), gui_scaled(100u), gui_scaled(19u),
                            "SYSTEM MONITOR", PANEL_LINE_COLOR);
    panel_put_centered_text(panel_file_x, (int)gui_scaled(6u), gui_scaled(40u), gui_scaled(19u),
                            "FILE", PANEL_TEXT_COLOR);
    panel_put_centered_text(panel_help_x, (int)gui_scaled(6u), gui_scaled(44u), gui_scaled(19u),
                            "HELP", PANEL_TEXT_COLOR);
    panel_put_centered_text(panel_shutdown_x, (int)gui_scaled(6u), gui_scaled(62u), gui_scaled(19u),
                            "SHUT", PANEL_LINE_COLOR);
    gsh_button_draw();
}

u8 panel_handle_click(int x, int y) {
    if (!panel_ready) return 0u;
    if (button_contains(&shutdown_button, x, y)) {
        power_system_shutdown();
        return 1u;
    }
    if (panel_system_monitor_button_contains(x, y)) {
        panel_system_monitor_button_click();
        return 1u;
    }
    if (panel_file_button_contains(x, y)) {
        panel_file_button_click();
        return 1u;
    }
    if (panel_help_button_contains(x, y)) {
        panel_help_button_click();
        return 1u;
    }
    return 0u;
}
