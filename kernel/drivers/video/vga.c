#include "vga.h"
#include "common.h"
#include "cpu.h"
#include <string.h>

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_COLOR_WHITE 0x0F

static volatile u16 *vga_buf = (u16*)0xB8000;
static u32 cursor_x = 0;
static u32 cursor_y = 0;
static u8 vga_current_color = VGA_COLOR_WHITE;

/* Bounded region for constrained output */
static int bounds_x = 0;
static int bounds_y = 0;
static int bounds_width = VGA_WIDTH;
static int bounds_height = VGA_HEIGHT;
static u8 bounds_enabled = 0;

void vga_update_cursor(void) {
    u16 pos = cursor_y * VGA_WIDTH + cursor_x;
    outb(0x3D4, 0x0F);
    for (volatile int i = 0; i < 10; i++);  /* Small delay for VGA controller */
    outb(0x3D5, (u8)(pos & 0xFF));
    for (volatile int i = 0; i < 10; i++);  /* Small delay for VGA controller */
    outb(0x3D4, 0x0E);
    for (volatile int i = 0; i < 10; i++);  /* Small delay for VGA controller */
    outb(0x3D5, (u8)((pos >> 8) & 0xFF));
    for (volatile int i = 0; i < 10; i++);  /* Small delay for VGA controller */
}

static void scroll(void) {
    if (bounds_enabled) {
        /* Scroll only within bounded region */
        for (int y = bounds_y; y < bounds_y + bounds_height - 1; y++) {
            for (int x = bounds_x; x < bounds_x + bounds_width; x++) {
                vga_write_cell(x, y, vga_read_cell(x, y + 1) & 0xFF, (vga_read_cell(x, y + 1) >> 8) & 0xFF);
            }
        }
        /* Clear last line in bounded region */
        for (int x = bounds_x; x < bounds_x + bounds_width; x++) {
            vga_write_cell(x, bounds_y + bounds_height - 1, ' ', VGA_COLOR_WHITE);
        }
    } else {
        /* Scroll full screen */
        volatile u32 y, x;
        for (y = 1; y < VGA_HEIGHT; y++) {
            u32 src = y * VGA_WIDTH;
            u32 dst = (y - 1) * VGA_WIDTH;
            for (x = 0; x < VGA_WIDTH; x++) {
                vga_buf[dst + x] = vga_buf[src + x];
            }
        }
        /* Clear the last line */
        u32 last = (VGA_HEIGHT - 1) * VGA_WIDTH;
        for (x = 0; x < VGA_WIDTH; x++) {
            vga_buf[last + x] = (u16)(' ' | (vga_current_color << 8));
        }
    }
}

void vga_clear(void) {
    if (bounds_enabled) {
        /* Clear only the bounded region */
        for (int y = bounds_y; y < bounds_y + bounds_height; y++) {
            for (int x = bounds_x; x < bounds_x + bounds_width; x++) {
                vga_write_cell(x, y, ' ', VGA_COLOR_WHITE);
            }
        }
        cursor_x = bounds_x;
        cursor_y = bounds_y;
    } else {
        /* Clear full screen */
        for (u32 i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
            vga_buf[i] = (u16)(' ' | (VGA_COLOR_WHITE << 8));
        }
        cursor_x = 0;
        cursor_y = 0;
        vga_current_color = VGA_COLOR_WHITE;
        vga_update_cursor();
    }
}

/* Clear the VGA text buffer without updating the hardware cursor (avoids port I/O).
 * Use this from contexts where VGA port access may not be safe. */
void vga_clear_no_update(void) {
    for (u32 i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buf[i] = (u16)(' ' | (VGA_COLOR_WHITE << 8));
    }
    cursor_x = 0;
    cursor_y = 0;
    vga_current_color = VGA_COLOR_WHITE;
}

void vga_set_color(unsigned char color) {
    vga_current_color = color;
}

void vga_move_cursor(int dx, int dy) {
    int new_x = (int)cursor_x + dx;
    int new_y = (int)cursor_y + dy;
    if (new_x < 0) new_x = 0;
    if (new_x >= VGA_WIDTH) new_x = VGA_WIDTH - 1;
    if (new_y < 0) new_y = 0;
    if (new_y >= VGA_HEIGHT) new_y = VGA_HEIGHT - 1;
    cursor_x = new_x;
    cursor_y = new_y;
    vga_update_cursor();
}

static void vga_putch_at(char c, u32 x, u32 y) {
    if (x < VGA_WIDTH && y < VGA_HEIGHT) {
        vga_buf[y * VGA_WIDTH + x] = (u16)(c | (vga_current_color << 8));
    }
}

void vga_backspace(void) {
    if (cursor_x > 0) {
        cursor_x--;
        vga_putch_at(' ', cursor_x, cursor_y);
    } else if (cursor_y > 0) {
        cursor_y--;
        cursor_x = VGA_WIDTH - 1;
        vga_putch_at(' ', cursor_x, cursor_y);
    }
    vga_update_cursor();
}

void vga_newline(void) {
    if (bounds_enabled) {
        cursor_x = bounds_x;
        cursor_y++;
        if (cursor_y >= bounds_y + bounds_height) {
            scroll();
            cursor_y = bounds_y + bounds_height - 1;
        }
    } else {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= VGA_HEIGHT) {
            scroll();
            cursor_y = VGA_HEIGHT - 1;
        }
        vga_update_cursor();
    }
}

void vga_init(void) {
    vga_clear();
}

void vga_putch(char c) {
    if (c == '\n') {
        vga_newline();
    } else if (c == '\t') {
        for (int i = 0; i < 4; i++) vga_putch(' ');
    } else if (c == '\b') {
        vga_backspace();
    } else if (c == '\r') {
        cursor_x = bounds_enabled ? bounds_x : 0;
        if (!bounds_enabled) vga_update_cursor();
    } else if (c >= 32 && c < 127) {
        int max_x = bounds_enabled ? bounds_x + bounds_width : VGA_WIDTH;
        if (cursor_x >= max_x) {
            vga_newline();
        }
        vga_putch_at(c, cursor_x, cursor_y);
        cursor_x++;
        if (cursor_x >= max_x) {
            vga_newline();
        }
        if (!bounds_enabled) vga_update_cursor();
    }
}

void vga_puts(const char *s) {
    while (*s) {
        vga_putch(*s++);
    }
}

/* Stubs for other functions */
void vga_scrollback_up(void) {
    /* Scroll up by one line (same as normal scroll, shift content down) */
    scroll();
}

void vga_scrollback_down(void) {
    /* Scroll down - opposite of normal scroll */
    /* Shift all rows down by one (row 0 -> row 1, etc.) */
    for (u32 y = VGA_HEIGHT - 1; y > 0; y--) {
        u32 src = (y - 1) * VGA_WIDTH;
        u32 dst = y * VGA_WIDTH;
        for (u32 x = 0; x < VGA_WIDTH; x++) {
            vga_buf[dst + x] = vga_buf[src + x];
        }
    }
    /* Clear the first line */
    for (u32 x = 0; x < VGA_WIDTH; x++) {
        vga_buf[x] = (u16)(' ' | (vga_current_color << 8));
    }
}

void vga_show_live_screen(void) {}

void vga_write_cell(int x, int y, char c, unsigned char color) {
    /* Respect bounds if enabled */
    if (bounds_enabled) {
        if (x < bounds_x || x >= bounds_x + bounds_width || 
            y < bounds_y || y >= bounds_y + bounds_height) {
            return;
        }
    } else {
        if (x < 0 || x >= VGA_WIDTH || y < 0 || y >= VGA_HEIGHT) {
            return;
        }
    }
    vga_buf[y * VGA_WIDTH + x] = (u16)(c | (color << 8));
}

unsigned short vga_read_cell(int x, int y) {
    if (x >= 0 && x < VGA_WIDTH && y >= 0 && y < VGA_HEIGHT) {
        return vga_buf[y * VGA_WIDTH + x];
    }
    return 0;
}

void vga_move_hardware_cursor(int x, int y) {
    if (x >= 0 && x < VGA_WIDTH && y >= 0 && y < VGA_HEIGHT) {
        cursor_x = x;
        cursor_y = y;
        vga_update_cursor();
    }
}

void vga_get_hardware_cursor(int *x, int *y) {
    if (x) *x = cursor_x;
    if (y) *y = cursor_y;
}

void vga_disable_hardware_cursor(void) {
    outb(0x3D4, 0x0A);
    for (volatile int i = 0; i < 10; i++);
    outb(0x3D5, 0x20);
    for (volatile int i = 0; i < 10; i++);
}

void vga_enable_hardware_cursor(void) {
    outb(0x3D4, 0x0A);
    for (volatile int i = 0; i < 10; i++);
    outb(0x3D5, 0x00);
    for (volatile int i = 0; i < 10; i++);
}

void vga_draw_button_box(int x, int y, int width, int height, unsigned char color) {
    for (int dy = 0; dy < height; dy++)
        for (int dx = 0; dx < width; dx++)
            vga_write_cell(x + dx, y + dy, ' ', color);
}

void vga_draw_button_text(int x, int y, const char *text, unsigned char color) {
    int px = x;
    while (*text && px < VGA_WIDTH) {
        vga_write_cell(px++, y, *text++, color);
    }
}

void vga_draw_button(int x, int y, int width, int height, const char *text,
                     unsigned char text_color, unsigned char bg_color) {
    vga_draw_button_box(x, y, width, height, bg_color);
    vga_draw_button_text(x, y, text, text_color);
}
/* Bounded region support for shell output */
void vga_set_bounds(int x, int y, int width, int height) {
    bounds_x = x;
    bounds_y = y;
    bounds_width = width;
    bounds_height = height;
    bounds_enabled = 1;
    cursor_x = bounds_x;
    cursor_y = bounds_y;
}

void vga_clear_bounds(void) {
    bounds_enabled = 0;
    cursor_x = 0;
    cursor_y = 0;
}

void vga_enable_paging(void) {
    vga_buf = (volatile u16 *)0xC00B8000;
}
