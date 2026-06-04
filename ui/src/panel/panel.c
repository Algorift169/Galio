#include "panel/panel.h"
#include "panel/clock.h"
#include "panel/date.h"
#include "vga.h"
#include "kernel_time.h"
#include "lib/string.h"
#include "kprintf.h"
#include <string.h>
#include "drivers/pit.h"

#define VGA_WIDTH 80


void panel_draw_header(void) {
    /* Use wall-clock DateTime for the header display */
    DateTime now = kernel_time_get_datetime();
    u32 wall_seconds = now.hour * 3600u + now.minute * 60u + now.second;

    char time_str[16];
    char date_str[32];
    panel_format_time(wall_seconds, time_str);
    panel_format_date(&now, date_str);
    
    /* Draw top line - Line 0: separator */
    vga_set_color(PANEL_COLOR_RED);
    
    /* Line 1: Status bar content */
    vga_set_color(PANEL_COLOR_BLUE);
    vga_puts("[Galio]");
    vga_putch(' ');
    vga_puts("[GSH]");
    
    /* Calculate spacing to push time to the right */
    u32 left_len = 8 + 5;  /* "[Galio]" (7) + space(1) + "[GSH]" (5) = 13 */
    u32 right_len = strlen(date_str) + strlen(time_str) + 7; /* "[HH:MM:SS] [DDd HH:MM]" */
    u32 space_len = (VGA_WIDTH > (left_len + right_len)) ? (VGA_WIDTH - left_len - right_len) : 1;
    
    /* Print spacing */
    for (u32 i = 0; i < space_len + 2; i++) {
        vga_putch(' ');
    }
    
    /* Print uptime (date) */
    vga_putch('[');
    vga_puts(date_str);
    vga_puts("] ");
    
    /* Print time */
    vga_putch('[');
    vga_puts(time_str);
    vga_putch(']');
    
    //vga_newline();
    
    /* Line 2: Separator line */
    vga_set_color(PANEL_COLOR_RED);
    for (u32 i = 0; i < VGA_WIDTH; i++) {
        vga_putch('-');
    }
    vga_newline();
    
    /* Reset to white */
    vga_set_color(PANEL_COLOR_WHITE);
}

void panel_update(void) {
    /* Called periodically to refresh the panel */
    /* For now, redraw the whole header */
    panel_draw_header();
}

/* Timer callback: redraw header on PIT ticks (once per second is sufficient,
   but PIT runs at 100Hz; we keep a counter to only redraw every 100 ticks). */
static void panel_tick(registers_t *regs) {
    static u32 tick_count = 0;
    (void)regs;
    tick_count++;
    if (tick_count >= 100) {
        tick_count = 0;
        panel_draw_header();
    }
}

void panel_init(void) {
    /* Initialize panel and register periodic redraw */
    pit_install_callback(panel_tick);
}