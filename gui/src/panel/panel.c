#include "panel.h"
#include "button.h"
#include "clock.h"
#include "drawline.h"
#include "framebuffer.h"
#include "gsh_button.h"
#include "power/power.h"

#define PANEL_HEIGHT 32u
#define PANEL_COLOR FB_COLOR(18u, 28u, 42u)
#define PANEL_LINE_COLOR FB_COLOR(105u, 145u, 165u)
#define PANEL_TEXT_COLOR FB_COLOR(235u, 242u, 245u)
#define PANEL_BUTTON_COLOR FB_COLOR(30u, 46u, 60u)
#define PANEL_BUTTON_X 990u
#define PANEL_BUTTON_Y 6u
#define PANEL_BUTTON_WIDTH 28u
#define PANEL_BUTTON_HEIGHT 19u
#define PANEL_SYSTEM_MONITOR_X 650
#define PANEL_FILE_X 758
#define PANEL_EDIT_X 802
#define PANEL_GSH_X 846
#define PANEL_HELP_X 882

static button_t shutdown_button;
static u8 panel_ready;

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
    if (character >= '0' && character <= '9') glyph = panel_font_digit[character - '0'];
    else if (character >= 'A' && character <= 'Z') glyph = panel_font_letter[character - 'A'];
    if (!glyph) return;
    for (u32 row = 0u; row < 7u; row++) {
        for (u32 column = 0u; column < 5u; column++) {
            if (glyph[row] & (1u << (4u - column))) {
                fb_fill_rect((u32)(x + (int)column), (u32)(y + (int)row), 1u, 1u, color);
            }
        }
    }
}

static void panel_put_text(int x, int y, const char *text, u32 color) {
    while (text && *text) {
        if (*text != ':' && *text != '-' && *text != '/') panel_put_char(x, y, *text, color);
        x += 6;
        text++;
    }
}

void panel_draw_clock(void) {
    char date[11];
    char time[9];

    clock_format_datetime(date, time);

    fb_fill_rect(8u, 8u, 198u, 14u, PANEL_COLOR);
    panel_put_text(12, 12, "DATE", PANEL_LINE_COLOR);
    panel_put_text(42, 12, date, PANEL_TEXT_COLOR);
    panel_put_text(122, 12, "TIME", PANEL_LINE_COLOR);
    panel_put_text(152, 12, time, PANEL_TEXT_COLOR);
}

void panel_init(void) {
    button_init(&shutdown_button, "SHUTDOWN", PANEL_BUTTON_X, PANEL_BUTTON_Y,
                PANEL_BUTTON_WIDTH, PANEL_BUTTON_HEIGHT,
                PANEL_BUTTON_COLOR, PANEL_TEXT_COLOR);
    panel_system_monitor_button_init(PANEL_SYSTEM_MONITOR_X, 6);
    panel_file_button_init(PANEL_FILE_X, 6);
    panel_edit_button_init(PANEL_EDIT_X, 6);
    panel_help_button_init(PANEL_HELP_X, 6);
    gsh_button_set_position(PANEL_GSH_X, 6);
    panel_ready = 1u;
}

void panel_draw(void) {
    if (!panel_ready) return;
    fb_fill_rect(0u, 0u, 1024u, PANEL_HEIGHT, PANEL_COLOR);
    gui_draw_border(0, 0, 1024u, PANEL_HEIGHT, PANEL_LINE_COLOR);
    panel_draw_clock();
    panel_system_monitor_button_draw();
    panel_file_button_draw();
    panel_edit_button_draw();
    panel_help_button_draw();
    button_draw(&shutdown_button);
    panel_put_text(PANEL_SYSTEM_MONITOR_X + 6, 12, "SYSTEM MONITOR", PANEL_LINE_COLOR);
    panel_put_text(PANEL_FILE_X + 11, 12, "FILE", PANEL_LINE_COLOR);
    panel_put_text(PANEL_EDIT_X + 11, 12, "EDIT", PANEL_LINE_COLOR);
    panel_put_text(PANEL_HELP_X + 10, 12, "HELP", PANEL_LINE_COLOR);
    panel_put_text((int)PANEL_BUTTON_X + 2, (int)PANEL_BUTTON_Y + 6, "SHUT", PANEL_LINE_COLOR);
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
    if (panel_edit_button_contains(x, y)) {
        panel_edit_button_click();
        return 1u;
    }
    if (panel_help_button_contains(x, y)) {
        panel_help_button_click();
        return 1u;
    }
    return 0u;
}
