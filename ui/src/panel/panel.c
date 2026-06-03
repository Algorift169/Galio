#include "panel/panel.h"
#include "vga.h"
#include "drivers/pit.h"
#include "lib/string.h"
#include "kprintf.h"
#include <string.h>

#define VGA_WIDTH 80

/* Simple utility to convert u32 to decimal string */
static u32 utoa_digits(u32 num, char *buf, u32 min_width) {
    char temp[12];
    u32 len = 0;
    
    if (num == 0) {
        temp[len++] = '0';
    } else {
        while (num > 0 && len < sizeof(temp)) {
            temp[len++] = (char)('0' + (num % 10));
            num /= 10;
        }
    }
    
    /* Pad with zeros */
    u32 pad = (min_width > len) ? (min_width - len) : 0;
    for (u32 i = 0; i < pad; i++) {
        buf[i] = '0';
    }
    
    /* Copy reversed digits */
    for (u32 i = 0; i < len; i++) {
        buf[pad + i] = temp[len - 1 - i];
    }
    
    return pad + len;
}

/* Format seconds into readable time string HH:MM:SS */
static void format_time(u32 seconds, char *time_str) {
    u32 hours = (seconds / 3600) % 24;
    u32 minutes = (seconds / 60) % 60;
    u32 secs = seconds % 60;
    
    u32 pos = 0;
    pos += utoa_digits(hours, &time_str[pos], 2);
    time_str[pos++] = ':';
    pos += utoa_digits(minutes, &time_str[pos], 2);
    time_str[pos++] = ':';
    pos += utoa_digits(secs, &time_str[pos], 2);
    time_str[pos] = '\0';
}

/* Format uptime into date-like string */
static void format_date(u32 seconds, char *date_str) {
    u32 days = seconds / 86400;
    u32 hours = (seconds % 86400) / 3600;
    u32 minutes = (seconds % 3600) / 60;
    
    u32 pos = 0;
    if (days > 0) {
        pos += utoa_digits(days, &date_str[pos], 0);
        date_str[pos++] = 'd';
        date_str[pos++] = ' ';
    }
    pos += utoa_digits(hours, &date_str[pos], 2);
    date_str[pos++] = ':';
    pos += utoa_digits(minutes, &date_str[pos], 2);
    date_str[pos] = '\0';
}

void panel_init(void) {
    /* Initialize panel */
}

void panel_draw_header(void) {
    /* Get current time from PIT */
    u32 ticks = pit_get_ticks();
    u32 seconds = ticks / 100;
    
    char time_str[16];
    char date_str[32];
    format_time(seconds, time_str);
    format_date(seconds, date_str);
    
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