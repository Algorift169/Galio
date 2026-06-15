#include "panel/shell_region.h"
#include "vga.h"

#define SHELL_REGION_BUFFER_SIZE 2048

typedef struct {
    int x;
    int y;
    int width;
    int height;
    int cur_x;
    int cur_y;
    u8 color;
    u8 enabled;
} shell_region_t;

static shell_region_t shell_region = {
    .x = 0,
    .y = 0,
    .width = 0,
    .height = 0,
    .cur_x = 0,
    .cur_y = 0,
    .color = 0x0F,
    .enabled = 0
};

void shell_region_init(int x, int y, int width, int height) {
    shell_region.x = x + 1;  /* Account for left border */
    shell_region.y = y + 1;  /* Account for top border */
    shell_region.width = width - 2;  /* Account for borders */
    shell_region.height = height - 2;  /* Account for borders */
    shell_region.cur_x = shell_region.x;
    shell_region.cur_y = shell_region.y;
    shell_region.color = 0x0F;
    shell_region.enabled = 1;
    
    /* Clear the region */
    shell_region_clear();
}

void shell_region_clear(void) {
    if (!shell_region.enabled) return;
    
    for (int y = shell_region.y; y < shell_region.y + shell_region.height; y++) {
        for (int x = shell_region.x; x < shell_region.x + shell_region.width; x++) {
            vga_write_cell(x, y, ' ', 0x00);
        }
    }
    shell_region.cur_x = shell_region.x;
    shell_region.cur_y = shell_region.y;
}

static void shell_region_scroll_up(void) {
    if (!shell_region.enabled) return;
    
    /* Shift lines up */
    for (int y = shell_region.y; y < shell_region.y + shell_region.height - 1; y++) {
        for (int x = shell_region.x; x < shell_region.x + shell_region.width; x++) {
            u16 cell = vga_read_cell(x, y + 1);
            vga_write_cell(x, y, cell & 0xFF, (cell >> 8) & 0xFF);
        }
    }
    
    /* Clear last line */
    for (int x = shell_region.x; x < shell_region.x + shell_region.width; x++) {
        vga_write_cell(x, shell_region.y + shell_region.height - 1, ' ', 0x00);
    }
}

void shell_region_putch(char c) {
    if (!shell_region.enabled) return;
    
    if (c == '\n') {
        shell_region.cur_x = shell_region.x;
        shell_region.cur_y++;
        
        /* Scroll if reached bottom */
        if (shell_region.cur_y >= shell_region.y + shell_region.height) {
            shell_region_scroll_up();
            shell_region.cur_y = shell_region.y + shell_region.height - 1;
        }
        return;
    }
    
    if (c == '\r') {
        shell_region.cur_x = shell_region.x;
        return;
    }
    
    if (c == '\b') {
        if (shell_region.cur_x > shell_region.x) {
            shell_region.cur_x--;
            vga_write_cell(shell_region.cur_x, shell_region.cur_y, ' ', 0x00);
        }
        return;
    }
    
    if (c == '\t') {
        /* Tab = 4 spaces */
        for (int i = 0; i < 4; i++) {
            shell_region_putch(' ');
        }
        return;
    }
    
    /* Regular character */
    if (shell_region.cur_x >= shell_region.x + shell_region.width) {
        /* Wrap to next line */
        shell_region.cur_x = shell_region.x;
        shell_region.cur_y++;
        
        if (shell_region.cur_y >= shell_region.y + shell_region.height) {
            shell_region_scroll_up();
            shell_region.cur_y = shell_region.y + shell_region.height - 1;
        }
    }
    
    vga_write_cell(shell_region.cur_x, shell_region.cur_y, c, shell_region.color);
    shell_region.cur_x++;
}

void shell_region_puts(const char *s) {
    if (!s || !shell_region.enabled) return;
    
    for (int i = 0; s[i]; i++) {
        shell_region_putch(s[i]);
    }
}

void shell_region_set_color(u8 color) {
    shell_region.color = color;
}

void shell_region_get_bounds(int *x, int *y, int *width, int *height) {
    if (x) *x = shell_region.x;
    if (y) *y = shell_region.y;
    if (width) *width = shell_region.width;
    if (height) *height = shell_region.height;
}

void shell_region_set_enabled(u8 enabled) {
    shell_region.enabled = enabled;
}

u8 shell_region_is_enabled(void) {
    return shell_region.enabled;
}
