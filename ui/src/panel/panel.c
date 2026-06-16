#include "panel/panel.h"
#include "panel/clock.h"
#include "panel/date.h"
#include "panel/sysinfo.h"
#include "panel/fs_browser.h"
#include "panel/launch_region.h"
#include "buttons/galio.h"
#include "buttons/gsh.h"
#include "vga.h"
#include "kernel_time.h"
#include "lib/string.h"
#include "kprintf.h"
#include <string.h>
#include "drivers/pit.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

static u8 panel_enabled = 1;

void panel_set_enabled(u8 enabled) {
    panel_enabled = enabled;
}

static void uint_to_str(u32 num, char *buf, int buf_size) {
    if (buf_size < 2) return;
    char temp[16];
    int i = 0;
    if (num == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    while (num > 0 && i < 15) {
        temp[i++] = '0' + (num % 10);
        num /= 10;
    }
    int j = 0;
    for (int k = i - 1; k >= 0; k--) {
        if (j < buf_size - 1) buf[j++] = temp[k];
    }
    buf[j] = '\0';
}

void panel_draw_header(void) {
    if (!panel_enabled) {
        return;
    }
    
    sysinfo_t sysinfo = sysinfo_get();
    DateTime now = kernel_time_get_datetime();
    u32 wall_seconds = now.hour * 3600u + now.minute * 60u + now.second;

    char time_str[16];
    char date_str[32];
    panel_format_time(wall_seconds, time_str);
    panel_format_date(&now, date_str);
    
    /* ROW 0: Header with buttons and date/time */
    galio_button_draw(0, 0);
    gsh_button_draw(9, 0);
    
    vga_set_color(PANEL_COLOR_BLUE);
    int time_len = strlen(time_str);
    int date_len = strlen(date_str);
    int display_start = VGA_WIDTH - time_len - date_len - 5;
    if (display_start < 15) display_start = 15;
    
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
    
    /* ROW 1: Separator */
    vga_set_color(PANEL_COLOR_RED);
    for (int x = 0; x < VGA_WIDTH; x++) {
        vga_write_cell(x, 1, '_', PANEL_COLOR_RED);
    }
    
    /* ROW 2: Top border */
    for (int x = 0; x < 17; x++) {
        if (x < 16) vga_write_cell(x, 2, ' ', 0x0F);
        else vga_write_cell(x, 2, '|', PANEL_COLOR_RED);
    }
    
    /* ROW 3: CPU label - simplified */
    vga_set_color(PANEL_COLOR_WHITE);
    const char *cpu_label = "[cpu]";
    for (int i = 0; cpu_label[i] && i < 16; i++) vga_write_cell(i, 3, cpu_label[i], PANEL_COLOR_WHITE);
    
    /* ROW 4: CPU value */
    char cpu_str[5];
    uint_to_str(sysinfo.cpu_percent, cpu_str, sizeof(cpu_str));
    vga_set_color(PANEL_COLOR_RED);
    int x = 0;
    for (int i = 0; cpu_str[i] && x < 14; i++, x++) vga_write_cell(x, 4, cpu_str[i], PANEL_COLOR_RED);
    if (x < 15) vga_write_cell(x, 4, '%', PANEL_COLOR_RED);
    
    /* ROW 5: Separator */
    vga_set_color(PANEL_COLOR_RED);
    for (int i = 0; i < 16; i++) vga_write_cell(i, 5, '-', PANEL_COLOR_RED);
    vga_write_cell(16, 5, '|', PANEL_COLOR_RED);
    
    /* ROW 6: Memory label - simplified */
    vga_set_color(PANEL_COLOR_WHITE);
    const char *mem_label = "[memory]";
    for (int i = 0; mem_label[i] && i < 16; i++) vga_write_cell(i, 6, mem_label[i], PANEL_COLOR_WHITE);
    
    /* ROW 7: Memory percentage value */
    u32 mem_percent = (sysinfo.memory_used * 100) / sysinfo.memory_total;
    char mem_str[5];
    uint_to_str(mem_percent, mem_str, sizeof(mem_str));
    vga_set_color(PANEL_COLOR_RED);
    x = 0;
    for (int i = 0; mem_str[i] && x < 14; i++, x++) vga_write_cell(x, 7, mem_str[i], PANEL_COLOR_RED);
    if (x < 15) vga_write_cell(x, 7, '%', PANEL_COLOR_RED);
    
    /* ROW 8: Separator */
    vga_set_color(PANEL_COLOR_RED);
    for (int i = 0; i < 16; i++) vga_write_cell(i, 8, '-', PANEL_COLOR_RED);
    vga_write_cell(16, 8, '|', PANEL_COLOR_RED);
    
    /* ROW 9: Battery label */
    vga_set_color(PANEL_COLOR_WHITE);
    const char *bat_label = "[battery]";
    for (int i = 0; bat_label[i] && i < 16; i++) vga_write_cell(i, 9, bat_label[i], PANEL_COLOR_WHITE);
    
    /* ROW 10: Battery value */
    char bat_str[8];
    uint_to_str(sysinfo.battery_percent, bat_str, sizeof(bat_str));
    vga_set_color(PANEL_COLOR_RED);
    x = 0;
    for (int i = 0; bat_str[i] && x < 14; i++, x++) vga_write_cell(x, 10, bat_str[i], PANEL_COLOR_RED);
    if (x < 15) vga_write_cell(x++, 10, '%', PANEL_COLOR_RED);
    if (sysinfo.battery_charging && x < 15) {
        vga_write_cell(x++, 10, ' ', PANEL_COLOR_RED);
        if (x < 15) vga_write_cell(x, 10, '+', PANEL_COLOR_RED);
    }
    
    /* ROW 11: Separator */
    vga_set_color(PANEL_COLOR_RED);
    for (int i = 0; i < 16; i++) vga_write_cell(i, 11, '-', PANEL_COLOR_RED);
    vga_write_cell(16, 11, '+', PANEL_COLOR_RED);

    /* ROWS 12-16: Filesystem browser (dynamic list) */
    fs_browser_draw(0, 12, 16, 5);
    
    vga_set_color(PANEL_COLOR_RED);
    vga_set_color(PANEL_COLOR_RED);
    vga_write_cell(16, 16, '+', PANEL_COLOR_RED);

    /* Vertical separator line */
    vga_set_color(PANEL_COLOR_RED);
    for (int y = 2; y <= 16; y++) vga_write_cell(16, y, '|', PANEL_COLOR_RED);
    for (int y = 17; y < VGA_HEIGHT; y++) vga_write_cell(16, y, '|', PANEL_COLOR_RED);
    
    /* ROW 17: Bottom horizontal separator */
    vga_set_color(PANEL_COLOR_RED);
    for (int x = 17; x < VGA_WIDTH; x++) vga_write_cell(x, 17, '-', PANEL_COLOR_RED);
    
    /* ROWS 2-24, COLS 18-79: Launch region for tools/applications (empty, just border) */
    launch_region_draw();
}

void panel_update(void) {
    if (!panel_enabled) {
        return;
    }
    panel_draw_header();
}

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
    galio_button_init();
    gsh_button_init();
    sysinfo_init();
    fs_browser_init();
    
    /* Initialize launch region - middle section for tools (cols 18-79, rows 2-16) */
    launch_region_init(18, 2, 62, 16);
    
    /* Add some default tools */
    launch_region_add_tool("Galio", 'G', 0x0A);      /* Green */
    launch_region_add_tool("Shell", 'S', 0x0B);      /* Cyan */
    launch_region_add_tool("Editor", 'E', 0x0D);     /* Magenta */
    launch_region_add_tool("Files", 'F', 0x0E);      /* Yellow */
    launch_region_add_tool("System", 'Y', 0x09);     /* Light blue */
    launch_region_add_tool("Network", 'N', 0x0C);    /* Light red */
    
    pit_install_callback(panel_tick);
}
