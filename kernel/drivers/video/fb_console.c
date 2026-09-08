#include "fb_console.h"
#include "framebuffer.h"

#define FB_CONSOLE_GLYPH_WIDTH  8u
#define FB_CONSOLE_GLYPH_HEIGHT 8u
#define FB_CONSOLE_FOREGROUND   0x00A8FF68u
#define FB_CONSOLE_BACKGROUND   0x00000000u

static u32 console_columns;
static u32 console_rows;
static u32 console_cell_width;
static u32 console_cell_height;
static u32 console_column;
static u32 console_row;
static u8 console_ready;
static u8 console_cursor;

static u8 glyph_row(char character, u32 row) {
    static const u8 digits[10][7] = {
        {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
        {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}, {0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E},
        {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, {0x1F,0x10,0x10,0x1E,0x01,0x01,0x1E},
        {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
        {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}
    };
    static const u8 letters[26][7] = {
        {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}, {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
        {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
        {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
        {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}, {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
        {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}, {0x01,0x01,0x01,0x01,0x11,0x11,0x0E},
        {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
        {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}, {0x11,0x19,0x19,0x15,0x13,0x13,0x11},
        {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
        {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
        {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}, {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
        {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, {0x11,0x11,0x11,0x11,0x0A,0x0A,0x04},
        {0x11,0x11,0x11,0x15,0x15,0x1B,0x11}, {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
        {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}, {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}
    };

    if (row >= 7) return 0;
    if (character >= '0' && character <= '9') return digits[character - '0'][row];
    if (character >= 'a' && character <= 'z') character = (char)(character - 'a' + 'A');
    if (character >= 'A' && character <= 'Z') return letters[character - 'A'][row];
    switch (character) {
        case '.': return row == 6 ? 0x04 : 0;
        case ',': return row == 6 ? 0x06 : 0;
        case ':': return row == 2 || row == 5 ? 0x04 : 0;
        case ';': return row == 2 ? 0x04 : (row == 6 ? 0x06 : 0);
        case '-': return row == 3 ? 0x0E : 0;
        case '_': return row == 6 ? 0x1F : 0;
        case '/': return row < 7 ? (u8)(1u << (6u - row)) : 0;
        case '\\': return row < 7 ? (u8)(1u << row) : 0;
        case '=': return row == 2 || row == 4 ? 0x1F : 0;
        case '+': return row == 3 ? 0x1F : (row == 1 || row == 5 ? 0x04 : 0);
        case '!': return row < 5 ? 0x04 : (row == 6 ? 0x04 : 0);
        case '?': return row == 0 ? 0x0E : (row == 1 ? 0x11 : (row == 2 ? 0x02 : (row == 6 ? 0x04 : 0)));
        case '|': return 0x04;
        case '(': return row == 0 || row == 6 ? 0x02 : 0x04;
        case ')': return row == 0 || row == 6 ? 0x08 : 0x04;
        case '[': return row == 0 || row == 6 ? 0x0E : 0x08;
        case ']': return row == 0 || row == 6 ? 0x0E : 0x02;
        case '*': return row == 3 ? 0x0A : (row == 2 || row == 4 ? 0x04 : 0);
        default: return character == ' ' ? 0 : (row == 0 || row == 6 ? 0x1F : 0x11);
    }
}

static void draw_cursor(void) {
    if (!console_ready) return;
    fb_fill_rect(console_column * console_cell_width,
                 console_row * console_cell_height + console_cell_height - 2u,
                 console_cell_width, 2u, console_cursor ? FB_CONSOLE_FOREGROUND : FB_CONSOLE_BACKGROUND);
}

static void draw_character(char character) {
    u32 x = console_column * console_cell_width;
    u32 y = console_row * console_cell_height;
    fb_fill_rect(x, y, console_cell_width, console_cell_height, FB_CONSOLE_BACKGROUND);
    for (u32 row = 0; row < 7; row++) {
        u8 bits = glyph_row(character, row);
        for (u32 column = 0; column < 5; column++) {
            if (bits & (1u << (4u - column))) {
                fb_fill_rect(x + column + 1u, y + row, 1u, 1u, FB_CONSOLE_FOREGROUND);
            }
        }
    }
}

static void scroll_console(void) {
    u32 width;
    u32 height;
    fb_get_info(&width, &height, NULL, NULL);
    if (console_rows < 2) return;
    for (u32 row = 1; row < console_rows; row++) {
        for (u32 y = 0; y < console_cell_height; y++) {
            for (u32 x = 0; x < width; x++) {
                fb_put_pixel(x, (row - 1u) * console_cell_height + y,
                             fb_get_pixel(x, row * console_cell_height + y));
            }
        }
    }
    fb_fill_rect(0u, (console_rows - 1u) * console_cell_height,
                 width, height - (console_rows - 1u) * console_cell_height,
                 FB_CONSOLE_BACKGROUND);
}

void fb_console_init(void) {
    u32 width;
    u32 height;
    if (!fb_is_initialized()) return;
    fb_get_info(&width, &height, NULL, NULL);
    console_cell_width = FB_CONSOLE_GLYPH_WIDTH;
    console_cell_height = FB_CONSOLE_GLYPH_HEIGHT;
    console_columns = width / console_cell_width;
    console_rows = height / console_cell_height;
    console_column = 0;
    console_row = 0;
    console_cursor = 1;
    console_ready = console_columns > 0 && console_rows > 0;
    if (console_ready) fb_clear(FB_CONSOLE_BACKGROUND);
}

u8 fb_console_active(void) { return console_ready; }

void fb_console_putc(char character) {
    if (!console_ready) return;
    draw_cursor();
    if (character == '\n') {
        console_column = 0;
        console_row++;
    } else if (character == '\r') {
        console_column = 0;
    } else if (character == '\b') {
        if (console_column > 0) console_column--;
        draw_character(' ');
    } else if (character == '\t') {
        for (u32 i = 0; i < 4; i++) fb_console_putc(' ');
        return;
    } else if (character >= 32 && character < 127) {
        draw_character(character);
        console_column++;
        if (console_column >= console_columns) {
            console_column = 0;
            console_row++;
        }
    }
    if (console_row >= console_rows) {
        scroll_console();
        console_row = console_rows - 1;
    }
    draw_cursor();
}