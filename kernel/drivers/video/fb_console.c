#include "fb_console.h"
#include "framebuffer.h"

#define FB_CONSOLE_GLYPH_WIDTH  8u
#define FB_CONSOLE_GLYPH_HEIGHT 16u
#define FB_CONSOLE_GLYPH_SCALE  1u
#define FB_CONSOLE_FOREGROUND   0x00FFFFFFu
#define FB_CONSOLE_BACKGROUND   0x00000000u
#define FB_CONSOLE_MAX_COLUMNS  128u
#define FB_CONSOLE_MAX_ROWS     96u
#define FB_CONSOLE_SCROLLBACK_LINES 200u

static u32 console_columns;
static u32 console_rows;
static u32 console_cell_width;
static u32 console_cell_height;
static u32 console_column;
static u32 console_row;
static u8 console_ready;
static u8 console_cursor;
static u32 console_background = FB_CONSOLE_BACKGROUND;
static u32 console_foreground = FB_CONSOLE_FOREGROUND;
static u16 console_cells[FB_CONSOLE_MAX_ROWS][FB_CONSOLE_MAX_COLUMNS];
static u16 console_scrollback[FB_CONSOLE_SCROLLBACK_LINES][FB_CONSOLE_MAX_COLUMNS];
static u32 console_scrollback_head = 0u;
static u32 console_scrollback_count = 0u;
static u32 console_scroll_offset = 0u;
static u8 console_bounds_enabled;
static u32 console_bounds_x;
static u32 console_bounds_y;
static u32 console_bounds_width;
static u32 console_bounds_height;
static u16 console_live_snapshot[FB_CONSOLE_MAX_ROWS][FB_CONSOLE_MAX_COLUMNS];
static u8 console_live_snapshot_valid = 0u;
static u16 console_relocate_cells[FB_CONSOLE_MAX_ROWS][FB_CONSOLE_MAX_COLUMNS];

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
    static const u8 lowercase[26][7] = {
        {0x00,0x00,0x0E,0x01,0x0F,0x11,0x0F}, {0x10,0x10,0x1E,0x11,0x11,0x11,0x1E},
        {0x00,0x00,0x0E,0x11,0x10,0x11,0x0E}, {0x01,0x01,0x0F,0x11,0x11,0x11,0x0F},
        {0x00,0x00,0x0E,0x11,0x1F,0x10,0x0E}, {0x06,0x09,0x08,0x1E,0x08,0x08,0x08},
        {0x00,0x00,0x0E,0x11,0x11,0x0F,0x01}, {0x10,0x10,0x1E,0x11,0x11,0x11,0x11},
        {0x04,0x00,0x0C,0x04,0x04,0x04,0x0E}, {0x02,0x00,0x06,0x02,0x02,0x12,0x0C},
        {0x10,0x10,0x12,0x14,0x18,0x14,0x12}, {0x0C,0x04,0x04,0x04,0x04,0x04,0x0E},
        {0x00,0x00,0x1A,0x15,0x15,0x15,0x15}, {0x00,0x00,0x1E,0x11,0x11,0x11,0x11},
        {0x00,0x00,0x0E,0x11,0x11,0x11,0x0E}, {0x00,0x00,0x1E,0x11,0x11,0x1E,0x10},
        {0x00,0x00,0x0E,0x11,0x11,0x0F,0x01}, {0x00,0x00,0x16,0x19,0x10,0x10,0x10},
        {0x00,0x00,0x0F,0x10,0x0E,0x01,0x1E}, {0x08,0x08,0x1E,0x08,0x08,0x09,0x06},
        {0x00,0x00,0x11,0x11,0x11,0x13,0x0D}, {0x00,0x00,0x11,0x11,0x0A,0x0A,0x04},
        {0x00,0x00,0x11,0x15,0x15,0x15,0x0A}, {0x00,0x00,0x11,0x0A,0x04,0x0A,0x11},
        {0x00,0x00,0x11,0x11,0x0F,0x01,0x0E}, {0x00,0x00,0x1F,0x02,0x04,0x08,0x1F}
    };

    if (row >= 7) return 0;
    if (character >= '0' && character <= '9') return digits[character - '0'][row];
    if (character >= 'a' && character <= 'z') return lowercase[character - 'a'][row];
    if (character >= 'A' && character <= 'Z') return letters[character - 'A'][row];
    switch (character) {
        case '.': return row == 6 ? 0x04 : 0;
        case ',': return row == 6 ? 0x06 : 0;
        case ':': return row == 2 || row == 5 ? 0x04 : 0;
        case ';': return row == 2 ? 0x04 : (row == 6 ? 0x06 : 0);
        case '-': return row == 3 ? 0x0E : 0;
        case '_': return row == 6 ? 0x1F : 0;
        case '/': return row < 7 ? (u8)(1u << row) : 0;
        case '\\': return row < 7 ? (u8)(1u << (6u - row)) : 0;
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
        case '@': return row == 0 ? 0x0E : (row == 1 ? 0x11 : (row == 2 ? 0x17 : (row == 3 ? 0x15 : (row == 4 ? 0x17 : (row == 5 ? 0x10 : 0x0E)))));
        case '#': return row == 1 || row == 5 ? 0x0A : (row == 2 || row == 3 || row == 4 ? 0x1F : 0x0A);
        case '$': return row == 0 || row == 6 ? 0x04 : (row == 1 ? 0x0E : (row == 2 ? 0x10 : (row == 3 ? 0x0E : (row == 4 ? 0x01 : 0x1E))));
        case '%': return row == 0 || row == 1 ? 0x19 : (row == 2 ? 0x02 : (row == 3 ? 0x04 : (row == 4 ? 0x08 : 0x13)));
        case '&': return row == 0 ? 0x0C : (row == 1 ? 0x12 : (row == 2 ? 0x0C : (row == 3 ? 0x0A : (row == 4 ? 0x15 : (row == 5 ? 0x12 : 0x0D)))));
        case '~': return row == 2 ? 0x0D : (row == 3 ? 0x16 : 0);
        case '"': return row < 2 ? 0x0A : 0;
        case '\'': return row < 2 ? 0x04 : 0;
        case '`': return row == 0 ? 0x08 : 0;
        case '^': return row == 0 ? 0x04 : (row == 1 ? 0x0A : 0);
        case '<': return row == 3 ? 0x04 : (row == 2 || row == 4 ? 0x02 : 0);
        case '>': return row == 3 ? 0x04 : (row == 2 || row == 4 ? 0x08 : 0);
        default: return 0;
    }
}

static void draw_cursor(void) {
    /* The shell redraws its own cursor cell. Do not paint a persistent
     * underline over the next character position. */
    (void)console_cursor;
}

static void draw_glyph(u32 x, u32 y, char character, u32 background) {
    fb_fill_rect(x, y, console_cell_width, console_cell_height, background);
    for (u32 row = 0; row < 7u; row++) {
        u8 bits = glyph_row(character, row);
        for (u32 column = 0; column < 5u; column++) {
            if (bits & (1u << (4u - column))) {
                fb_fill_rect(x + 1u + column * FB_CONSOLE_GLYPH_SCALE,
                             y + 4u + row * FB_CONSOLE_GLYPH_SCALE,
                             FB_CONSOLE_GLYPH_SCALE,
                             FB_CONSOLE_GLYPH_SCALE,
                             console_foreground);
            }
        }
    }
}

static void draw_character(char character) {
    u32 x = console_column * console_cell_width;
    u32 y = console_row * console_cell_height;
    console_cells[console_row][console_column] = (u16)(character | (0x0Fu << 8u));
    draw_glyph(x, y, character, console_background);
}

static void fb_console_draw_cell(u32 x, u32 y, u16 cell) {
    char character = (char)(cell & 0xFFu);
    u8 attr = (u8)(cell >> 8);
    u32 background = console_background;

    if (character == '\0') character = ' ';
    fb_fill_rect(x * console_cell_width,
                 y * console_cell_height,
                 console_cell_width,
                 console_cell_height,
                 background);
    if (character != ' ') {
        draw_glyph(x * console_cell_width,
                   y * console_cell_height,
                   character,
                   background);
    }

    (void)attr;
}

static void fb_console_render_visible(void) {
    if (!console_ready) return;
    u32 first_row = console_bounds_enabled ? console_bounds_y : 0u;
    u32 last_row = console_bounds_enabled ? console_bounds_y + console_bounds_height : console_rows;
    u32 first_column = console_bounds_enabled ? console_bounds_x : 0u;
    u32 last_column = console_bounds_enabled ? console_bounds_x + console_bounds_width : console_columns;
    for (u32 row = first_row; row < last_row; row++) {
        for (u32 column = first_column; column < last_column; column++) {
            fb_console_draw_cell(column, row, console_cells[row][column]);
        }
    }
}

void fb_console_redraw(void) {
    fb_console_render_visible();
}

void fb_console_relocate(int old_x, int old_y, int new_x, int new_y, int width, int height) {
    if (!console_ready || width <= 0 || height <= 0) return;
    if (old_x < 0 || old_y < 0 || new_x < 0 || new_y < 0) return;
    if (old_x + width > (int)console_columns || new_x + width > (int)console_columns ||
        old_y + height > (int)console_rows || new_y + height > (int)console_rows) return;

    for (int row = 0; row < height; row++) {
        for (int column = 0; column < width; column++) {
            console_relocate_cells[row][column] =
                console_cells[old_y + row][old_x + column];
            console_cells[old_y + row][old_x + column] = (u16)' ';
        }
    }
    for (int row = 0; row < height; row++) {
        for (int column = 0; column < width; column++) {
            console_cells[new_y + row][new_x + column] =
                console_relocate_cells[row][column];
        }
    }
    console_column = (u32)new_x;
    console_row = (u32)new_y;
}

static void fb_console_capture_live_snapshot(void) {
    for (u32 row = 0; row < console_rows; row++) {
        for (u32 column = 0; column < console_columns; column++) {
            console_live_snapshot[row][column] = console_cells[row][column];
        }
    }
    console_live_snapshot_valid = 1u;
}

static void fb_console_render_scrollback_view(void) {
    if (!console_ready) return;

    if (console_scroll_offset == 0u) {
        fb_console_render_visible();
        return;
    }

    if (!console_live_snapshot_valid) {
        fb_console_capture_live_snapshot();
    }

    u32 history_rows = console_scroll_offset < console_rows ? console_scroll_offset : console_rows;

    for (u32 row = 0; row < console_rows; row++) {
        for (u32 column = 0; column < console_columns; column++) {
            u16 cell;
            if (row < history_rows) {
                u32 lines_back = console_scroll_offset - row;
                u32 idx = (console_scrollback_head + FB_CONSOLE_SCROLLBACK_LINES - lines_back) % FB_CONSOLE_SCROLLBACK_LINES;
                cell = console_scrollback[idx][column];
            } else {
                u32 live_row = row - history_rows;
                cell = console_live_snapshot[live_row][column];
            }
            fb_console_draw_cell(column, row, cell);
        }
    }
}

static void scroll_console(void) {
    u32 width;
    u32 height;
    fb_get_info(&width, &height, NULL, NULL);
    u32 first_row = console_bounds_enabled ? console_bounds_y : 0u;
    u32 last_row = console_bounds_enabled ? console_bounds_y + console_bounds_height : console_rows;
    u32 first_column = console_bounds_enabled ? console_bounds_x : 0u;
    u32 last_column = console_bounds_enabled ? console_bounds_x + console_bounds_width : console_columns;
    if (last_row <= first_row + 1u) return;

    for (u32 row = first_row; row + 1u < last_row; row++) {
        for (u32 column = first_column; column < last_column; column++) {
            console_cells[row][column] = console_cells[row + 1u][column];
        }
    }
    for (u32 column = first_column; column < last_column; column++) {
        console_cells[last_row - 1u][column] = (u16)' ';
    }

    for (u32 x = first_column; x < last_column; x++) {
        console_scrollback[console_scrollback_head][x] = console_cells[0][x];
    }
    console_scrollback_head = (console_scrollback_head + 1u) % FB_CONSOLE_SCROLLBACK_LINES;
    if (console_scrollback_count < FB_CONSOLE_SCROLLBACK_LINES) {
        console_scrollback_count++;
    }

    if (console_scroll_offset != 0u) {
        console_scroll_offset = 0u;
    }

    fb_console_render_visible();
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
    console_bounds_enabled = 0u;
    console_cursor = 1;
    console_ready = console_columns > 0 && console_rows > 0;
    if (console_ready) fb_clear(FB_CONSOLE_BACKGROUND);
}

u8 fb_console_active(void) { return console_ready; }

void fb_console_putc(char character) {
    if (!console_ready) return;
    draw_cursor();
    if (character == '\n') {
        console_column = console_bounds_enabled ? console_bounds_x : 0u;
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
        u32 right = console_bounds_enabled ? console_bounds_x + console_bounds_width : console_columns;
        if (console_column >= right) {
            console_column = console_bounds_enabled ? console_bounds_x : 0u;
            console_row++;
        }
    }
    u32 bottom = console_bounds_enabled ? console_bounds_y + console_bounds_height : console_rows;
    if (console_row >= bottom) {
        scroll_console();
        console_row = bottom - 1u;
    }
    draw_cursor();
}

void fb_console_clear(u32 color) {
    if (!console_ready) return;
    console_background = color;
    fb_clear(color);
    for (u32 row = 0; row < FB_CONSOLE_MAX_ROWS; row++) {
        for (u32 column = 0; column < FB_CONSOLE_MAX_COLUMNS; column++) {
            console_cells[row][column] = (u16)' ';
        }
    }
    for (u32 row = 0; row < FB_CONSOLE_SCROLLBACK_LINES; row++) {
        for (u32 column = 0; column < FB_CONSOLE_MAX_COLUMNS; column++) {
            console_scrollback[row][column] = (u16)' ';
        }
    }
    console_scrollback_head = 0u;
    console_scrollback_count = 0u;
    console_scroll_offset = 0u;
    console_live_snapshot_valid = 0u;
    console_column = 0;
    console_row = 0;
    console_bounds_enabled = 0u;
}

void fb_console_set_background(u32 color) {
    if (console_ready) console_background = color;
}

u32 fb_console_get_background(void) {
    return console_background;
}

void fb_console_set_color(u8 color) {
    if (!console_ready) return;
    console_foreground = (color & 0x0Fu) == 0x0Eu ? 0x00D4AF37u : 0x00FFFFFFu;
}

void fb_console_set_foreground(u32 color) {
    if (console_ready) console_foreground = color;
}

void fb_console_set_bounds(int x, int y, int width, int height) {
    if (!console_ready || x < 0 || y < 0 || width <= 0 || height <= 0) return;
    console_bounds_x = (u32)x;
    console_bounds_y = (u32)y;
    console_bounds_width = (u32)width;
    console_bounds_height = (u32)height;
    if (console_bounds_x + console_bounds_width > console_columns) {
        console_bounds_width = console_columns - console_bounds_x;
    }
    if (console_bounds_y + console_bounds_height > console_rows) {
        console_bounds_height = console_rows - console_bounds_y;
    }
    console_bounds_enabled = console_bounds_width > 0u && console_bounds_height > 0u;
    console_column = console_bounds_x;
    console_row = console_bounds_y;
}

void fb_console_clear_bounds(void) {
    if (!console_ready || !console_bounds_enabled) return;
    fb_console_clear_region();
    console_bounds_enabled = 0u;
}

void fb_console_clear_region(void) {
    if (!console_ready || !console_bounds_enabled) return;
    fb_console_clear_active_region();
}

void fb_console_clear_active_region(void) {
    if (!console_ready || console_bounds_width == 0u || console_bounds_height == 0u) return;
    for (u32 row = console_bounds_y; row < console_bounds_y + console_bounds_height; row++) {
        for (u32 column = console_bounds_x; column < console_bounds_x + console_bounds_width; column++) {
            console_cells[row][column] = (u16)' ';
        }
    }
    fb_fill_rect(console_bounds_x * console_cell_width,
                 console_bounds_y * console_cell_height,
                 console_bounds_width * console_cell_width,
                 console_bounds_height * console_cell_height,
                 console_background);
    console_column = console_bounds_x;
    console_row = console_bounds_y;
}

void fb_console_begin_prompt_line(void) {
    if (!console_ready) return;
    u32 left = console_bounds_enabled ? console_bounds_x : 0u;
    u32 width = console_bounds_enabled ? console_bounds_width : console_columns;
    fb_fill_rect(left * console_cell_width, console_row * console_cell_height,
                 width * console_cell_width, console_cell_height,
                 console_background);
}

void fb_console_scroll_up(void) {
    if (!console_ready || console_scrollback_count == 0u) return;

    u32 step = 3u;
    if (console_scroll_offset + step > console_scrollback_count) {
        console_scroll_offset = console_scrollback_count;
    } else {
        console_scroll_offset += step;
    }

    if (!console_live_snapshot_valid) {
        fb_console_capture_live_snapshot();
    }

    fb_console_render_scrollback_view();
}

void fb_console_scroll_down(void) {
    if (!console_ready || console_scroll_offset == 0u) return;

    u32 step = 3u;
    if (console_scroll_offset <= step) {
        console_scroll_offset = 0u;
        fb_console_render_visible();
        return;
    }

    console_scroll_offset -= step;
    fb_console_render_scrollback_view();
}

void fb_console_write_cell(int x, int y, char character, u8 color) {
    if (!console_ready || x < 0 || y < 0 ||
        (u32)x >= console_columns || (u32)y >= console_rows) return;
    console_cells[y][x] = (u16)((u8)character | ((u16)color << 8));
    if ((color & 0x0Fu) != 0u) {
        draw_glyph((u32)x * console_cell_width, (u32)y * console_cell_height,
                   character, console_background);
    } else {
        fb_fill_rect((u32)x * console_cell_width, (u32)y * console_cell_height,
                     console_cell_width, console_cell_height, console_background);
    }
}

void fb_console_write_cursor_cell(int x, int y, char character) {
    u32 saved_foreground;
    if (!console_ready || x < 0 || y < 0 ||
        (u32)x >= console_columns || (u32)y >= console_rows) return;
    console_cells[y][x] = (u16)((u8)character | 0xF000u);
    fb_fill_rect((u32)x * console_cell_width, (u32)y * console_cell_height,
                 console_cell_width, console_cell_height, 0x00FFFFFFu);
    saved_foreground = console_foreground;
    console_foreground = 0x00000000u;
    draw_glyph((u32)x * console_cell_width, (u32)y * console_cell_height,
               character, 0x00FFFFFFu);
    console_foreground = saved_foreground;
}

u16 fb_console_read_cell(int x, int y) {
    if (!console_ready || x < 0 || y < 0 ||
        (u32)x >= console_columns || (u32)y >= console_rows) return (u16)' ';
    return console_cells[y][x];
}

void fb_console_set_cursor(int x, int y) {
    if (!console_ready) return;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    int left = console_bounds_enabled ? (int)console_bounds_x : 0;
    int top = console_bounds_enabled ? (int)console_bounds_y : 0;
    int right = console_bounds_enabled ? left + (int)console_bounds_width - 1 : (int)console_columns - 1;
    int bottom = console_bounds_enabled ? top + (int)console_bounds_height - 1 : (int)console_rows - 1;
    if (x < left) x = left;
    if (y < top) y = top;
    if (x > right) x = right;
    if (y > bottom) y = bottom;
    console_column = (u32)x;
    console_row = (u32)y;
}

void fb_console_get_cursor(int *x, int *y) {
    if (x) *x = console_ready ? (int)console_column : 0;
    if (y) *y = console_ready ? (int)console_row : 0;
}