#include "mouse/cursor.h"
#include "display_output.h"

#include "mouse/mouse.h"
#include "desktop.h"
#include "gsh_button.h"
#include "apps_container.h"
#include "spike.h"
#include "panel.h"
#include "framebuffer.h"
#include "srver/server.h"

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
    "............",
    "#...........",
    "##..........",
    "###.........",
    "####........",
    "#####.......",
    "######......",
    "#######.....",
    "########....",
    "#########...",
    "##########..",
    "###########.",
    "###########.",
    "##########..",
    "#########...",
    "########....",
    "#######.....",
    "######.....",
    "#####......"
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

    // Handle cursor visibility and position updates to 
    // prevent flickering and ensure the cursor is drawn correctly.
    if (!cursor_visible) {
        cursor_visible = 1u;
        save_cursor_background();
        draw_cursor();
    }

    if (x != cursor_x || y != cursor_y) {

        restore_cursor_background();

        cursor_x = x;
        cursor_y = y;

        const display_output_t *output = display_output_get();
        int max_x = output->width > 12u ? (int)output->width - 12 : 0;
        int max_y = output->height > 18u ? (int)output->height - 18 : 0;
        if (cursor_x < 0)
            cursor_x = 0;

        if (cursor_y < 0)
            cursor_y = 0;

        if (cursor_x > max_x)
            cursor_x = max_x;

        if (cursor_y > max_y)
            cursor_y = max_y;

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

    /* Press-edge detection for the mouse button handshake. We only fire on
       the transition from not-pressed to pressed, which prevents the shell
       launcher from retriggering while the user is dragging/holding. */
    u8 left_pressed = (buttons & 0x01u) && !(previous_buttons & 0x01u);
    u8 right_pressed = (buttons & 0x02u) && !(previous_buttons & 0x02u);

    if (left_pressed) {
            if (cursor_y >= display_output_get()->usable_y) {
        display_server_focus_at_point(cursor_x, cursor_y);
            }
        spike_window_handle_pointer(cursor_x, cursor_y);
        if (!panel_handle_click(cursor_x, cursor_y) &&
            gsh_button_is_minimized_icon_at(cursor_x, cursor_y)) {
            gsh_button_click();
        } else if (!panel_handle_click(cursor_x, cursor_y) &&
                   apps_container_one_contains_spike(cursor_x, cursor_y)) {
            spike_window_restore();
        } else if (!panel_handle_click(cursor_x, cursor_y) &&
            !gsh_button_is_any_terminal_control_at(cursor_x, cursor_y) &&
            gsh_button_contains(cursor_x, cursor_y)) {
            gsh_button_click();
        }
    } else if (right_pressed) {
        /* Surface a stable right-click branch without letting the right
           button masquerade as a left-button activation. Desktop runners can
           attach a context-open path here if needed. */
        desktop_handle_click(x, y);
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

    const display_output_t *output = display_output_get();
    int max_x = output->width > 12u ? (int)output->width - 12 : 0;
    int max_y = output->height > 18u ? (int)output->height - 18 : 0;

    if (x > max_x)
        x = max_x;

    if (y > max_y)
        y = max_y;

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
    if (!cursor_visible) {
        return;
    }
    restore_cursor_background();
    cursor_visible = 0u;
}

void cursor_hide(void)
{
    if (!cursor_visible) {
        return;
    }
    restore_cursor_background();
    cursor_visible = 0u;
}

void cursor_show(void)
{
    if (cursor_visible) {
        return;
    }
    save_cursor_background();
    cursor_visible = 1u;
    draw_cursor();
}