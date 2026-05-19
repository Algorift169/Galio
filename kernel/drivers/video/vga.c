#include "vga.h"
#include "common.h"
#include "cpu.h"
#include <string.h>

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_COLOR_WHITE 0x0F
#define SCROLLBACK_STEP 5

static volatile u16 *vga_buf = (u16*)0xB8000;
static u32 cursor_x = 0;
static u32 cursor_y = 0;

/* Current foreground+background attribute byte (default: white on black) */
static u8 vga_current_color = VGA_COLOR_WHITE;

#define SCROLLBACK_LINES 512
static char scrollback[SCROLLBACK_LINES][VGA_WIDTH + 1];
static u32 scrollback_head = 0;
static u32 scrollback_count = 0;
static u32 scrollback_offset = 0;
static u8 scrollback_mode = 0;
static char current_line[VGA_WIDTH + 1];
static u32 current_line_len = 0;

static void scrollback_push_line(void) {
    if (current_line_len >= VGA_WIDTH) {
        current_line[VGA_WIDTH] = '\0';
    } else {
        current_line[current_line_len] = '\0';
    }

    if (scrollback_count < SCROLLBACK_LINES) {
        u32 idx = (scrollback_head + scrollback_count) % SCROLLBACK_LINES;
        strncpy(scrollback[idx], current_line, VGA_WIDTH + 1);
        scrollback_count++;
    } else {
        strncpy(scrollback[scrollback_head], current_line, VGA_WIDTH + 1);
        scrollback_head = (scrollback_head + 1) % SCROLLBACK_LINES;
    }

    current_line_len = 0;
    current_line[0] = '\0';
    scrollback_offset = 0;
}

static void redraw_scrollback(void) {
    u32 line_count = scrollback_count;
    u32 start = 0;

    if (line_count > VGA_HEIGHT) {
        u32 max_offset = line_count - VGA_HEIGHT;
        if (scrollback_offset > max_offset) {
            scrollback_offset = max_offset;
        }
        start = max_offset - scrollback_offset;
    }

    for (u32 y = 0; y < VGA_HEIGHT; y++) {
        u32 line_index = start + y;
        const char *line = (line_index < line_count)
            ? scrollback[(scrollback_head + line_index) % SCROLLBACK_LINES]
            : NULL;

        for (u32 x = 0; x < VGA_WIDTH; x++) {
            char ch = ' ';
            if (line && x < strlen(line)) {
                ch = line[x];
            }
            vga_buf[y * VGA_WIDTH + x] = (u16)(ch | (VGA_COLOR_WHITE << 8));
        }
    }

    cursor_x = 0;
    cursor_y = VGA_HEIGHT - 1;
    vga_update_cursor();
}

static void ensure_live_screen(void) {
    if (scrollback_mode) {
        scrollback_mode = 0;
        redraw_scrollback();
    }
}

void vga_update_cursor(void) {
    u16 pos = cursor_y * VGA_WIDTH + cursor_x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (u8)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (u8)((pos >> 8) & 0xFF));
}

static void scroll(void) {
    /* Move all rows up by one */
    for (u32 y = 1; y < VGA_HEIGHT; y++) {
        for (u32 x = 0; x < VGA_WIDTH; x++) {
            vga_buf[(y-1) * VGA_WIDTH + x] = vga_buf[y * VGA_WIDTH + x];
        }
    }
    
    /* Clear last line (always white — it is blank infrastructure) */
    for (u32 x = 0; x < VGA_WIDTH; x++) {
        vga_buf[(VGA_HEIGHT-1) * VGA_WIDTH + x] = (u16)(' ' | (VGA_COLOR_WHITE << 8));
    }
    
    /* Adjust cursor position */
    if (cursor_y > 0) {
        cursor_y--;
    }
}

void vga_clear(void) {
    for (u32 i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buf[i] = (u16)(' ' | (VGA_COLOR_WHITE << 8));
    }
    cursor_x = 0;
    cursor_y = 0;
    current_line_len = 0;
    current_line[0] = '\0';
    scrollback_mode = 0;
    scrollback_offset = 0;
    vga_current_color = VGA_COLOR_WHITE;   /* reset color on clear */
    vga_update_cursor();
}

/* Set the current character color.
 * color is a standard VGA attribute byte:
 *   high nibble = background, low nibble = foreground
 *   e.g. 0x0A = light green on black
 *        0x0C = light red on black
 *        0x0E = yellow on black
 *        0x0F = white on black  */
void vga_set_color(unsigned char color) {
    vga_current_color = color;
}

void vga_move_cursor(int dx, int dy) {
    if (dx < 0) {
        u32 delta = (u32)(-dx);
        cursor_x = (cursor_x < delta) ? 0 : cursor_x - delta;
    } else {
        cursor_x += (u32)dx;
        if (cursor_x >= VGA_WIDTH) cursor_x = VGA_WIDTH - 1;
    }

    if (dy < 0) {
        u32 delta = (u32)(-dy);
        cursor_y = (cursor_y < delta) ? 0 : cursor_y - delta;
    } else {
        cursor_y += (u32)dy;
        if (cursor_y >= VGA_HEIGHT) cursor_y = VGA_HEIGHT - 1;
    }
    
    vga_update_cursor();
}

/* Uses vga_current_color instead of the hardcoded white */
static void vga_putch_at(char c, u32 x, u32 y) {
    if (x < VGA_WIDTH && y < VGA_HEIGHT) {
        vga_buf[y * VGA_WIDTH + x] = (u16)(c | (vga_current_color << 8));
    }
}

void vga_backspace(void) {
    ensure_live_screen();
    if (cursor_x > 0) {
        cursor_x--;
        vga_putch_at(' ', cursor_x, cursor_y);
        if (current_line_len > 0) {
            current_line_len--;
            current_line[current_line_len] = '\0';
        }
    } else if (cursor_y > 0) {
        cursor_y--;
        cursor_x = VGA_WIDTH - 1;
        vga_putch_at(' ', cursor_x, cursor_y);
        if (current_line_len > 0) {
            current_line_len--;
            current_line[current_line_len] = '\0';
        }
    }
    vga_update_cursor();
}

void vga_newline(void) {
    ensure_live_screen();
    scrollback_push_line();

    cursor_x = 0;
    cursor_y++;
    
    if (cursor_y >= VGA_HEIGHT) {
        scroll();
        cursor_y = VGA_HEIGHT - 1;
    }
    vga_update_cursor();
}

void vga_init(void) {
    vga_clear();
}

void vga_putch(char c) {
    if (c == '\n') {
        vga_newline();
    } else if (c == '\t') {
        /* Tab = 4 spaces */
        for (int i = 0; i < 4; i++) {
            vga_putch(' ');
        }
    } else if (c == '\b') {
        vga_backspace();
    } else if (c >= 32 && c < 127) {
        ensure_live_screen();
        /* Printable character */
        vga_putch_at(c, cursor_x, cursor_y);
        if (current_line_len < VGA_WIDTH) {
            current_line[current_line_len++] = c;
            current_line[current_line_len] = '\0';
        }
        cursor_x++;
        
        if (cursor_x >= VGA_WIDTH) {
            vga_newline();
        }
        vga_update_cursor();
    }
}

void vga_scrollback_up(void) {
    if (scrollback_count <= VGA_HEIGHT) return;
    scrollback_mode = 1;
    u32 max_offset = scrollback_count - VGA_HEIGHT;
    if (scrollback_offset + SCROLLBACK_STEP >= max_offset) {
        scrollback_offset = max_offset;
    } else {
        scrollback_offset += SCROLLBACK_STEP;
    }
    redraw_scrollback();
}

void vga_scrollback_down(void) {
    if (!scrollback_mode) return;
    if (scrollback_offset <= SCROLLBACK_STEP) {
        scrollback_offset = 0;
        scrollback_mode = 0;
    } else {
        scrollback_offset -= SCROLLBACK_STEP;
    }
    redraw_scrollback();
}

void vga_show_live_screen(void) {
    if (!scrollback_mode) return;
    scrollback_offset = 0;
    scrollback_mode = 0;
    redraw_scrollback();
}

void vga_puts(const char *s) {
    while (*s) {
        vga_putch(*s++);
    }
}