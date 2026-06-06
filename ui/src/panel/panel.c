#include "panel/panel.h"
#include "panel/clock.h"
#include "panel/date.h"
#include "buttons/galio.h"
#include "buttons/gsh.h"
#include "vga.h"
#include "kernel_time.h"
#include "lib/string.h"
#include "kprintf.h"
#include <string.h>
#include "drivers/pit.h"

#define VGA_WIDTH 80

static u8 panel_enabled = 1;

void panel_set_enabled(u8 enabled) {
    panel_enabled = enabled;
}

void panel_draw_header(void) {
    if (!panel_enabled) {
        return;
    }
    /* Use wall-clock DateTime for the header display */
    DateTime now = kernel_time_get_datetime();
    u32 wall_seconds = now.hour * 3600u + now.minute * 60u + now.second;

    char time_str[16];
    char date_str[32];
    panel_format_time(wall_seconds, time_str);
    panel_format_date(&now, date_str);
    
    /* Draw buttons at top row with blue highlighting */
    galio_button_draw(0, 0);
    gsh_button_draw(9, 0);
    
    /* Draw time/date at the right side using vga_write_cell to avoid cursor interference */
    vga_set_color(PANEL_COLOR_BLUE);
    
    int time_len = strlen(time_str);
    int date_len = strlen(date_str);
    int display_start = VGA_WIDTH - time_len - date_len - 5;  /* "[DD] [HH:MM:SS]" format */
    
    if (display_start < 15) display_start = 15;  /* Don't overwrite buttons area */
    
    /* Draw date and time using vga_write_cell */
    int pos = display_start;
    vga_write_cell(pos++, 0, '[', PANEL_COLOR_BLUE);
    for (int i = 0; i < date_len && pos < VGA_WIDTH; i++) {
        vga_write_cell(pos++, 0, date_str[i], PANEL_COLOR_BLUE);
    }
    vga_write_cell(pos++, 0, ']', PANEL_COLOR_BLUE);
    vga_write_cell(pos++, 0, ' ', PANEL_COLOR_BLUE);
    vga_write_cell(pos++, 0, '[', PANEL_COLOR_BLUE);
    for (int i = 0; i < time_len && pos < VGA_WIDTH; i++) {
        vga_write_cell(pos++, 0, time_str[i], PANEL_COLOR_BLUE);
    }
    if (pos < VGA_WIDTH) vga_write_cell(pos++, 0, ']', PANEL_COLOR_BLUE);
    
    /* Draw separator line on row 1 */
    vga_set_color(PANEL_COLOR_RED);
    for (int x = 0; x < VGA_WIDTH; x++) {
        vga_write_cell(x, 1, '-', PANEL_COLOR_RED);
    }
    
    /* Reset color and position cursor to row 2 for shell output */
    vga_set_color(PANEL_COLOR_WHITE);
}

void panel_update(void) {
    /* Called periodically to refresh the panel */
    if (!panel_enabled) {
        return;
    }
    panel_draw_header();
}

/* Timer callback: redraw header on PIT ticks (once per second is sufficient,
   but PIT runs at 100Hz; we keep a counter to only redraw every 100 ticks). */
static void panel_tick(registers_t *regs) {
    if (!panel_enabled) {
        return;
    }

    static u32 tick_count = 0;
    (void)regs;
    tick_count++;
    if (tick_count >= 100) {
        tick_count = 0;
        panel_draw_header();
    }
}

void panel_init(void) {
    /* Initialize buttons */
    galio_button_init();
    gsh_button_init();
    
    /* Initialize panel and register periodic redraw */
    pit_install_callback(panel_tick);
}