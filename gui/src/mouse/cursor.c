#include "mouse/cursor.h"

#include "mouse/mouse.h"
#include "desktop.h"
#include "gsh_button.h"
#include "framebuffer.h"

static int cursor_x;
static int cursor_y;
static u8 cursor_visible;
static u8 previous_buttons;

/*
 * Cursor size:
 *   Width  = 12 pixels
 *   Height = 18 pixels
 *
 * '.' = transparent
 * '+' = white fill
 * '#' = black outline
 */
static u32 cursor_background[12u * 18u];
static const char cursor_shape[18][13] = {
    "#...........",
    "##...........",
    "#+#..........",
    "#++#.........",
    "#+++#........",
    "#++++#.......",
    "#++++++#......",
    "#+++++++#.....",
    "#++++++++#....",
    "#++++++++++#...",
    "#+++++++++++#..",
    "#+++++++++++#..",
    "#++++++++++++#...",
    "#++++++++++#....",
    ".#+++++++++#....",
    ".....#++++#.....",
    ".....#++++#.....",
    ".....#++++#.....",
    ".....#++++#.....",
    "......#++++#.....",
    "......#++++#......",
    "........#++#.......",
    "....... #++#........",
    ".........#++#........",
    ".........#++#........"
    ".........#++#........"
    ".........#+#........"
    ".........#+#........"
    "..........##........"
};
static void save_cursor_background(void)
{
    for (u32 row = 0u; row < 18u; row++) {
        for (u32 column = 0u; column < 12u; column++) {
            cursor_background[row * 12u + column] =
                fb_get_pixel(
                    (u32)cursor_x + column,
                    (u32)cursor_y + row
                );
        }
    }
}

static void restore_cursor_background(void)
{
    for (u32 row = 0u; row < 18u; row++) {
        for (u32 column = 0u; column < 12u; column++) {
            fb_put_pixel(
                (u32)cursor_x + column,
                (u32)cursor_y + row,
                cursor_background[row * 12u + column]
            );
        }
    }
}

static void draw_cursor(void)
{
    if (!cursor_visible)
        return;

    for (u32 row = 0u; row < 18u; row++) {
        for (u32 column = 0u; column < 12u; column++) {

            char pixel = cursor_shape[row][column];

            if (pixel == '.')
                continue;

            u32 color;

            if (pixel == '+') {
                color = 0x00FFFFFFu;
            } else {
                color = 0x00000000u;
            }

            fb_put_pixel(
                (u32)cursor_x + column,
                (u32)cursor_y + row,
                color
            );
        }
    }
}

void cursor_init(void)
{
    cursor_x = 512;
    cursor_y = 384;

    cursor_visible = 1u;
    previous_buttons = 0u;

    save_cursor_background();
    draw_cursor();
}

void cursor_poll(void)
{
    mouse_poll_position();

    int x;
    int y;

    u8 buttons = mouse_get_buttons();

    mouse_get_position(&x, &y);

    if (x != cursor_x || y != cursor_y) {

        restore_cursor_background();

        cursor_x = x;
        cursor_y = y;

        /*
         * Keep the complete 12x18 cursor inside
         * the 1024x768 framebuffer.
         */
        if (cursor_x < 0)
            cursor_x = 0;

        if (cursor_y < 0)
            cursor_y = 0;

        if (cursor_x > 1024 - 12)
            cursor_x = 1024 - 12;

        if (cursor_y > 768 - 18)
            cursor_y = 768 - 18;

        save_cursor_background();
        draw_cursor();
    }

    gsh_button_set_hovered(
        gsh_button_contains(cursor_x, cursor_y)
    );

    if (gsh_button_contains(x, y) &&
        buttons != previous_buttons) {

        gsh_button_set_hovered(1u);
    }

    if ((buttons & 1u) &&
        !(previous_buttons & 1u) &&
        gsh_button_contains(cursor_x, cursor_y)) {

        gsh_button_click();
    }

    previous_buttons = buttons;
}

void cursor_refresh_desktop(void)
{
    if (cursor_visible) {
        restore_cursor_background();
    }

    desktop_draw();

    if (cursor_visible) {
        save_cursor_background();
        draw_cursor();
    }
}

void cursor_rebase(void)
{
    if (!cursor_visible)
        return;

    save_cursor_background();
    draw_cursor();
}

void cursor_set_position(int x, int y)
{
    if (x < 0)
        x = 0;

    if (y < 0)
        y = 0;

    /*
     * Cursor is 12x18, so clamp its TOP-LEFT position
     * rather than allowing the cursor to go outside
     * the framebuffer.
     */
    if (x > 1024 - 12)
        x = 1024 - 12;

    if (y > 768 - 18)
        y = 768 - 18;

    cursor_x = x;
    cursor_y = y;
}

void cursor_move(int dx, int dy)
{
    cursor_set_position(
        cursor_x + dx,
        cursor_y + dy
    );
}

void cursor_get_position(int *x, int *y)
{
    if (x)
        *x = cursor_x;

    if (y)
        *y = cursor_y;
}

void cursor_deactivate(void)
{
    cursor_visible = 0u;
}

void cursor_hide(void)
{
    cursor_visible = 0u;
}

void cursor_show(void)
{
    cursor_visible = 1u;

    save_cursor_background();
    draw_cursor();
}