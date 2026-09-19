#include "gsh_button.h"
#include "button.h"
#include "window.h"
#include "win-border.h"
#include "terminal_window.h"
#include "desktop.h"
#include "panel.h"
#include "mouse/cursor.h"
#include "fb_console.h"
#include "framebuffer.h"
#include "vga.h"
#include "shell.h"
#include "srver/client.h"
#include "srver/server.h"
#include "srver/resources.h"
#include "display_wrapper.h"
#include "display_output.h"
#include "gui_scale.h"
#include "gsh_icon.h"

extern void apps_container_one_draw(void);

#define GSH_BUTTON_TEXT_COLOR FB_COLOR(105u, 145u, 165u)
#define GSH_PANEL_COLOR FB_COLOR(105u, 145u, 165u)
#define GSH_MAX_EXTRA_WINDOWS 3u

static button_t gsh_launch_button;
static terminal_window_t gsh_window;
static int gsh_button_x = 20;
static int gsh_button_y = 20;
static u8 gsh_window_active = 0u;
static u8 gsh_launch_pressed = 0u;
static u8 gsh_hovered = 0u;
static u32 gsh_server_client_id = 0u;
static u32 gsh_window_id = 0u;
static u32 gsh_server_window_id = 0u;
static terminal_window_t gsh_extra_windows[GSH_MAX_EXTRA_WINDOWS];
static u32 gsh_extra_window_ids[GSH_MAX_EXTRA_WINDOWS];
static terminal_window_t *gsh_active_terminal = &gsh_window;
static u8 gsh_pointer_buttons = 0u;
static u8 gsh_monitor_active = 0u;
static u32 gsh_terminal_event = 0u;

extern void apps_container_one_set_app_count(u32 app_count);
extern u8 apps_container_one_contains_gsh(int x, int y);

static void repaint_exposed_wallpaper(int old_x, int old_y, u32 width, u32 height,
                                      int new_x, int new_y) {
    int old_right = old_x + (int)width;
    int old_bottom = old_y + (int)height;
    int new_right = new_x + (int)width;
    int new_bottom = new_y + (int)height;
    int overlap_left = old_x > new_x ? old_x : new_x;
    int overlap_top = old_y > new_y ? old_y : new_y;
    int overlap_right = old_right < new_right ? old_right : new_right;
    int overlap_bottom = old_bottom < new_bottom ? old_bottom : new_bottom;
    int panel_top = 0;
    int panel_bottom = 32;

    if (overlap_left >= overlap_right || overlap_top >= overlap_bottom) {
        display_wrapper_draw_region((u32)old_x, (u32)old_y, width, height);
        if (old_y < panel_bottom && old_bottom > panel_top) panel_draw();
        return;
    }
    if (old_y < overlap_top) {
        display_wrapper_draw_region((u32)old_x, (u32)old_y, width,
                                    (u32)(overlap_top - old_y));
    }
    if (overlap_bottom < old_bottom) {
        display_wrapper_draw_region((u32)old_x, (u32)overlap_bottom, width,
                                    (u32)(old_bottom - overlap_bottom));
    }
    if (old_x < overlap_left) {
        display_wrapper_draw_region((u32)old_x, (u32)overlap_top,
                                    (u32)(overlap_left - old_x),
                                    (u32)(overlap_bottom - overlap_top));
    }
    if (overlap_right < old_right) {
        display_wrapper_draw_region((u32)overlap_right, (u32)overlap_top,
                                    (u32)(old_right - overlap_right),
                                    (u32)(overlap_bottom - overlap_top));
    }

    if (overlap_top < panel_bottom && overlap_bottom > panel_top) {
        panel_draw();
        gsh_button_draw();
    }

    apps_container_one_draw();
}

static u32 gsh_window_id_for_terminal(const terminal_window_t *terminal);


static void gsh_save_terminal_content(terminal_window_t *terminal) {
    int cursor_x;
    int cursor_y;

    if (!terminal || !terminal->visible) return;
    fb_console_get_cursor(&cursor_x, &cursor_y);
    terminal->console_cursor_x = cursor_x;
    terminal->console_cursor_y = cursor_y;
    for (u32 row = 0u; row < terminal->console_height && row < TERMINAL_WINDOW_MAX_ROWS; row++) {
        for (u32 column = 0u; column < terminal->console_width && column < TERMINAL_WINDOW_MAX_COLUMNS; column++) {
            terminal->console_cells[row * TERMINAL_WINDOW_MAX_COLUMNS + column] =
                fb_console_read_cell(terminal->console_x + (int)column,
                                     terminal->console_y + (int)row);
        }
    }
    terminal->content_valid = 1u;
}

static void gsh_restore_terminal_cursor(const terminal_window_t *terminal) {
    if (!terminal || !terminal->visible || !terminal->content_valid) return;
    fb_console_set_cursor(terminal->console_cursor_x, terminal->console_cursor_y);
}

static void gsh_restore_terminal_content(terminal_window_t *terminal) {
    if (!terminal || !terminal->visible || !terminal->content_valid) return;
    for (u32 row = 0u; row < terminal->console_height && row < TERMINAL_WINDOW_MAX_ROWS; row++) {
        for (u32 column = 0u; column < terminal->console_width && column < TERMINAL_WINDOW_MAX_COLUMNS; column++) {
            u16 cell = terminal->console_cells[row * TERMINAL_WINDOW_MAX_COLUMNS + column];
            fb_console_write_cell(terminal->console_x + (int)column,
                                  terminal->console_y + (int)row,
                                  (char)(cell & 0xFFu), (u8)(cell >> 8));
        }
    }
}

static void gsh_draw_terminal_content(terminal_window_t *terminal) {
    if (!terminal || !terminal->visible) return;
    terminal_window_draw(terminal);
    gsh_restore_terminal_content(terminal);
    win_border_draw_outline(&terminal->window, terminal->window.border_color);
}

static void redraw_other_gsh_windows(const terminal_window_t *active_terminal) {
    terminal_window_t *terminals[GSH_MAX_EXTRA_WINDOWS + 1u];
    u32 window_ids[GSH_MAX_EXTRA_WINDOWS + 1u];
    u32 count = 0u;
    u32 index;

    if (active_terminal != &gsh_window && gsh_window.visible && gsh_window_id != 0u &&
        display_server_resource_validate_window(gsh_window_id)) {
        terminals[count] = &gsh_window;
        window_ids[count++] = gsh_window_id;
    }
    for (index = 0u; index < GSH_MAX_EXTRA_WINDOWS; index++) {
        if (&gsh_extra_windows[index] != active_terminal &&
            gsh_extra_window_ids[index] != 0u && gsh_extra_windows[index].visible &&
            display_server_resource_validate_window(gsh_extra_window_ids[index])) {
            terminals[count] = &gsh_extra_windows[index];
            window_ids[count++] = gsh_extra_window_ids[index];
        }
    }

    for (index = 1u; index < count; index++) {
        terminal_window_t *terminal = terminals[index];
        u32 window_id = window_ids[index];
        u32 position = index;
        display_server_window_t *window = display_server_resources_find_window(window_id);
        u32 z_order = window ? window->z_order : 0u;
        while (position > 0u) {
            display_server_window_t *previous = display_server_resources_find_window(
                window_ids[position - 1u]);
            if (!previous || previous->z_order <= z_order) break;
            terminals[position] = terminals[position - 1u];
            window_ids[position] = window_ids[position - 1u];
            position--;
        }
        terminals[position] = terminal;
        window_ids[position] = window_id;
    }

    for (index = 0u; index < count; index++) {
        gsh_draw_terminal_content(terminals[index]);
    }
}

void gsh_button_redraw_windows(void) {
    redraw_other_gsh_windows((terminal_window_t *)0);
}

u8 gsh_button_owns_window(u32 window_id) {
    u32 index;

    if (window_id == 0u) return 0u;
    if (window_id == gsh_window_id) return 1u;
    for (index = 0u; index < GSH_MAX_EXTRA_WINDOWS; index++) {
        if (window_id == gsh_extra_window_ids[index]) return 1u;
    }
    return 0u;
}
u8 gsh_button_is_terminal_control_at(int x, int y) {
    return (u8)(gsh_window_active && gsh_active_terminal &&
                terminal_window_control_at(gsh_active_terminal, x, y) != TERMINAL_CONTROL_NONE);
}
u8 gsh_button_is_any_terminal_control_at(int x, int y) {
    u32 index;

    if (gsh_window.visible && terminal_window_control_at(&gsh_window, x, y) != TERMINAL_CONTROL_NONE) {
        return 1u;
    }
    for (index = 0u; index < GSH_MAX_EXTRA_WINDOWS; index++) {
        if (gsh_extra_windows[index].visible &&
            terminal_window_control_at(&gsh_extra_windows[index], x, y) != TERMINAL_CONTROL_NONE) {
            return 1u;
        }
    }
    return 0u;
}

u8 gsh_button_is_minimized_icon_at(int x, int y) {
    return (u8)(gsh_window_active && !gsh_window.visible &&
                apps_container_one_contains_gsh(x, y));
}

static void gsh_save_active_content(void) {
    gsh_save_terminal_content(gsh_active_terminal);
}

void gsh_button_draw_icon(int x, int y, u32 size) {
    gsh_icon_draw(x, y, size);
}

void gsh_button_draw_icon_colored(int x, int y, u32 size, u32 color) {
    gsh_icon_draw_with_color(x, y, size, color);
}

void gsh_button_init(void) {
    u32 index;

    button_init(&gsh_launch_button, "", 20u, 20u, gui_scaled(28u), gui_scaled(19u),
                FB_COLOR(18, 28, 42), 0x00FFFFFFu);
    gsh_button_x = 20;
    gsh_button_y = 20;
    terminal_window_init(&gsh_window, "gsh", 170u, 90u, 620u, 360u);
    gsh_window_active = 0u;
    gsh_launch_pressed = 0u;
    gsh_pointer_buttons = 0u;
    gsh_monitor_active = 0u;
    gsh_terminal_event = 0u;
    gsh_window_id = 0u;
    gsh_server_window_id = 0u;
    gsh_active_terminal = &gsh_window;
    for (index = 0u; index < GSH_MAX_EXTRA_WINDOWS; index++) {
        gsh_extra_window_ids[index] = 0u;
    }
    if (gsh_server_client_id == 0u) {
        gsh_server_client_id = display_server_client_connect();
    }
}

void gsh_button_draw(void) {
    u32 background = gsh_hovered ? FB_COLOR(42, 62, 78) : gsh_launch_button.background;
    fb_fill_rect((u32)gsh_button_x, (u32)gsh_button_y,
                 gsh_launch_button.width, gsh_launch_button.height, background);
    fb_draw_rect((u32)gsh_button_x, (u32)gsh_button_y,
                 gsh_launch_button.width, gsh_launch_button.height,
                 GSH_PANEL_COLOR);
    gsh_icon_draw((int)gsh_button_x + 3, (int)gsh_button_y, 21u);
    if (gsh_hovered) {
        fb_draw_rect((u32)gsh_button_x + 2u, (u32)gsh_button_y + 2u,
                     gsh_launch_button.width - 4u, gsh_launch_button.height - 4u,
                     GSH_PANEL_COLOR);
    }
}

void gsh_button_set_position(int x, int y) {
    gsh_button_x = x;
    gsh_button_y = y;
    gsh_launch_button.x = x;
    gsh_launch_button.y = y;
}

u8 gsh_button_contains(int x, int y) {
    return button_contains(&gsh_launch_button, x, y);
}

void gsh_button_set_hovered(u8 hovered) {
    gsh_hovered = hovered ? 1u : 0u;
}

u8 gsh_button_is_input_enabled(void) {
    /* Only the live, visible, unminimized terminal should consume shell keyboard
     * input. Once a terminal is minimized or hidden, it must stop receiving
     * keystrokes without terminating the shell loop itself. */
    return gsh_window_active && gsh_server_window_id != 0u &&
           gsh_active_terminal != (terminal_window_t *)0u &&
           gsh_active_terminal->visible &&
           !gsh_active_terminal->minimized &&
           display_server_get_active_window_id() == gsh_server_window_id;
}

u32 gsh_button_get_active_window_id(void) {
    return gsh_server_window_id;
}

u32 gsh_button_get_terminal_event(void) {
    return gsh_terminal_event;
}

void gsh_button_set_monitor_active(u8 active) {
    gsh_monitor_active = active ? 1u : 0u;
}

static void gsh_open_extra_window(void) {
    u32 index;
    int x;
    int y;

    for (index = 0u; index < GSH_MAX_EXTRA_WINDOWS; index++) {
        if (gsh_extra_window_ids[index] != 0u) continue;

        x = 220 + (int)(index * 48u);
        y = 125 + (int)(index * 36u);
        terminal_window_init(&gsh_extra_windows[index], "gsh", (u32)x, (u32)y,
                             620u, 360u);
        gsh_extra_window_ids[index] = display_server_client_create_window(
            gsh_server_client_id, "gsh", x, y, 620u, 360u);
        if (gsh_extra_window_ids[index] == 0u) return;

        terminal_window_open(&gsh_extra_windows[index]);
        terminal_window_draw(&gsh_extra_windows[index]);
        terminal_window_clear_content(&gsh_extra_windows[index]);
        gsh_save_terminal_content(&gsh_extra_windows[index]);
        gsh_active_terminal = &gsh_extra_windows[index];
        gsh_server_window_id = gsh_extra_window_ids[index];
        gsh_terminal_event++;
        display_server_focus_window(gsh_server_window_id);
        terminal_window_set_bounds(gsh_active_terminal);
        shell_set_exit_region(gsh_active_terminal->window.x +
                      (int)gsh_active_terminal->window.width - 16,
                      gsh_active_terminal->window.y + 2,
                      12, 12);
        if (!gsh_monitor_active) shell_redraw_terminal();
        cursor_show();
        return;
    }
}

static terminal_window_t *gsh_terminal_for_window_id(u32 window_id) {
    u32 index;

    if (window_id != 0u && window_id == gsh_window_id) return &gsh_window;
    for (index = 0u; index < GSH_MAX_EXTRA_WINDOWS; index++) {
        if (gsh_extra_window_ids[index] == window_id) {
            return &gsh_extra_windows[index];
        }
    }
    return (terminal_window_t *)0;
}

static u32 gsh_window_id_for_terminal(const terminal_window_t *terminal) {
    u32 index;

    if (terminal == &gsh_window) return gsh_window_id;
    for (index = 0u; index < GSH_MAX_EXTRA_WINDOWS; index++) {
        if (terminal == &gsh_extra_windows[index]) {
            return gsh_extra_window_ids[index];
        }
    }
    return 0u;
}

static void gsh_focus_terminal(terminal_window_t *terminal, u32 window_id) {
    if (!terminal || window_id == 0u ||
        !display_server_resource_validate_window(window_id)) return;

    gsh_active_terminal = terminal;
    gsh_server_window_id = window_id;
    gsh_terminal_event++;
    display_server_focus_window(window_id);
    cursor_deactivate();
    gsh_draw_terminal_content(terminal);
    terminal_window_set_bounds(terminal);
    gsh_restore_terminal_cursor(terminal);
    shell_set_exit_region(terminal->window.x + (int)terminal->window.width - 16,
                          terminal->window.y + 2, 12, 12);
    cursor_show();
}


static u8 gsh_close_active_window(int x, int y) {
    terminal_window_t *terminal = gsh_active_terminal;
    u32 window_id;
    u32 index;
    u32 best_id = 0u;
    u32 best_z = 0u;

    if (!terminal || !terminal_window_exit_contains(terminal, x, y)) return 0u;

    if (terminal == &gsh_window) {
        /* The shell loop still owns the main console until it returns. */
        shell_request_exit();
        return 1u;
    }

    window_id = gsh_window_id_for_terminal(terminal);
    gsh_save_terminal_content(terminal);
    cursor_deactivate();
    terminal_window_close(terminal);
    display_server_client_destroy_window(gsh_server_client_id, window_id);

    for (index = 0u; index < GSH_MAX_EXTRA_WINDOWS; index++) {
        if (&gsh_extra_windows[index] == terminal) {
            gsh_extra_window_ids[index] = 0u;
            break;
        }
    }

    for (index = 0u; index < GSH_MAX_EXTRA_WINDOWS; index++) {
        display_server_window_t *window;
        if (gsh_extra_window_ids[index] == 0u ||
            !display_server_resource_validate_window(gsh_extra_window_ids[index])) {
            continue;
        }
        window = display_server_resources_find_window(gsh_extra_window_ids[index]);
        if (window && window->z_order >= best_z) {
            best_z = window->z_order;
            best_id = window->id;
        }
    }

    desktop_draw();
    redraw_other_gsh_windows((terminal_window_t *)0);
    if (best_id != 0u) {
        gsh_active_terminal = gsh_terminal_for_window_id(best_id);
        gsh_server_window_id = best_id;
        gsh_focus_terminal(gsh_active_terminal, best_id);
    } else {
        gsh_active_terminal = &gsh_window;
        gsh_server_window_id = 0u;
        shell_clear_exit_region();
    }
    cursor_show();
    return 1u;
}

static u8 gsh_toggle_fullscreen(terminal_window_t *terminal) {
    const display_output_t *output;
    u32 window_id;
    int old_x;
    int old_y;
    u32 old_width;
    u32 old_height;
    u8 was_maximized;

    if (!terminal || !terminal->visible) return 0u;
    output = display_output_get();
    window_id = gsh_window_id_for_terminal(terminal);
    old_x = terminal->window.x;
    old_y = terminal->window.y;
    old_width = terminal->window.width;
    old_height = terminal->window.height;
    was_maximized = terminal->maximized;
    if (terminal->maximized) {
        terminal->window.x = terminal->normal_x;
        terminal->window.y = terminal->normal_y;
        terminal->window.width = terminal->normal_width;
        terminal->window.height = terminal->normal_height;
        terminal->maximized = 0u;
    } else {
        terminal->normal_x = terminal->window.x;
        terminal->normal_y = terminal->window.y;
        terminal->normal_width = terminal->window.width;
        terminal->normal_height = terminal->window.height;
        terminal->window.x = output->usable_x;
        terminal->window.y = output->usable_y;
        terminal->window.width = output->usable_width;
        terminal->window.height = output->usable_height;
        terminal->maximized = 1u;
    }
    terminal_window_sync_layout(terminal);
    if (window_id != 0u) {
        display_server_client_move_window(gsh_server_client_id, window_id,
                                          terminal->window.x, terminal->window.y);
        display_server_resize_window(window_id, terminal->window.width,
                                     terminal->window.height);
    }
    if (was_maximized) {
        (void)old_x;
        (void)old_y;
        (void)old_width;
        (void)old_height;
        desktop_draw();
        redraw_other_gsh_windows(terminal);
    }
    cursor_deactivate();
    gsh_draw_terminal_content(terminal);
    terminal_window_set_bounds(terminal);
    gsh_restore_terminal_cursor(terminal);
    if (terminal == gsh_active_terminal) {
        shell_set_exit_region(terminal->window.x + (int)terminal->window.width - 16,
                              terminal->window.y + 2, 12, 12);
    }
    cursor_show();
    return 1u;
}

static u8 gsh_focus_existing_window(void) {
    u32 index;

    if (gsh_server_window_id != 0u &&
        display_server_resource_validate_window(gsh_server_window_id) &&
        gsh_active_terminal) {
        gsh_focus_terminal(gsh_active_terminal, gsh_server_window_id);
        return 1u;
    }

    for (index = 0u; index < GSH_MAX_EXTRA_WINDOWS; index++) {
        if (gsh_extra_window_ids[index] == 0u ||
            !display_server_resource_validate_window(gsh_extra_window_ids[index])) {
            continue;
        }
        gsh_server_window_id = gsh_extra_window_ids[index];
        gsh_active_terminal = &gsh_extra_windows[index];
        gsh_focus_terminal(gsh_active_terminal, gsh_server_window_id);
        return 1u;
    }

    return 0u;
}


void gsh_button_click(void) {
    if (gsh_window_active) {
        if (gsh_launch_pressed) return;
        if (gsh_active_terminal == &gsh_window && !gsh_active_terminal->visible) {
            apps_container_one_set_app_count(0u);
            desktop_draw();
            gsh_active_terminal->visible = 1u;
            gsh_active_terminal->minimized = 0u;
            if (gsh_server_window_id != 0u &&
                display_server_resource_validate_window(gsh_server_window_id)) {
                display_server_show_window(gsh_server_window_id);
                display_server_focus_window(gsh_server_window_id);
            }
            terminal_window_set_bounds(gsh_active_terminal);
            gsh_draw_terminal_content(gsh_active_terminal);
            shell_set_exit_region(gsh_active_terminal->window.x +
                                  (int)gsh_active_terminal->window.width - 16,
                                  gsh_active_terminal->window.y + 2, 12, 12);
            cursor_show();
            return;
        }
        gsh_save_active_content();
        gsh_open_extra_window();
        if (gsh_active_terminal != &gsh_window) {
            return;
        }
        if (gsh_focus_existing_window()) return;
        gsh_window_active = 0u;
        gsh_server_window_id = 0u;
        gsh_active_terminal = &gsh_window;
    }
    if (gsh_server_client_id == 0u) {
        gsh_server_client_id = display_server_client_connect();
    }
    if (gsh_server_window_id == 0u) {
        gsh_server_window_id = display_server_client_create_window(
            gsh_server_client_id,
            "gsh",
            gsh_window.window.x,
            gsh_window.window.y,
            gsh_window.window.width,
            gsh_window.window.height
        );
        gsh_window_id = gsh_server_window_id;
    }

    gsh_window_active = 1u;
    gsh_launch_pressed = 1u;
    gsh_active_terminal = &gsh_window;
    cursor_deactivate();
    terminal_window_open(gsh_active_terminal);
    terminal_window_set_bounds(gsh_active_terminal);
    terminal_window_draw(gsh_active_terminal);
    terminal_window_clear_content(gsh_active_terminal);
    cursor_show();
    shell_set_exit_region(gsh_active_terminal->window.x +
                          (int)gsh_active_terminal->window.width - 16,
                          gsh_active_terminal->window.y + 2, 12, 12);
    shell_run();
    shell_clear_exit_region();
    gsh_save_terminal_content(gsh_active_terminal);
    cursor_deactivate();
    terminal_window_close(gsh_active_terminal);

    if (gsh_server_window_id != 0u) {
        display_server_client_destroy_window(gsh_server_client_id, gsh_server_window_id);
        gsh_server_window_id = 0u;
    }

    desktop_draw();
    redraw_other_gsh_windows((terminal_window_t *)0);
    apps_container_one_set_app_count(0u);
    desktop_draw();
    cursor_show();
    gsh_window_active = 0u;
    gsh_active_terminal = &gsh_window;
}

void gsh_button_poll_pointer(int x, int y, u8 buttons) {
    u8 left_pressed = (buttons & 0x01u) && !(gsh_pointer_buttons & 0x01u);

    if (!(buttons & 0x01u)) gsh_launch_pressed = 0u;
    if (!gsh_window_active || !gsh_active_terminal) {
        cursor_show();
        gsh_pointer_buttons = buttons;
        return;
    }

    /* Minimized terminals are not interactive: ignore all pointer-driven window
     * actions so a background monitor such as cpu-spike cannot re-enter the
     * fullscreen/restore logic while hidden. */
    if (gsh_active_terminal->minimized) {
        cursor_show();
        gsh_pointer_buttons = buttons;
        return;
    }

    if (left_pressed) {
        u8 control = terminal_window_control_at(gsh_active_terminal, x, y);
        if (control == TERMINAL_CONTROL_FULLSCREEN) {
            gsh_launch_pressed = 1u;
            gsh_toggle_fullscreen(gsh_active_terminal);
            gsh_pointer_buttons = buttons;
            return;
        }
        if (control == TERMINAL_CONTROL_MINIMIZE) {
            gsh_launch_pressed = 1u;
            if (gsh_active_terminal == &gsh_window) {
                u32 window_id = gsh_server_window_id;
                gsh_save_terminal_content(gsh_active_terminal);
                /* Minimize must leave the shell alive, but remove it from the
                 * active input/focus path so the desktop can own input while the
                 * terminal remains hidden. Keep the window id so the shell can be
                 * restored later without losing its live session state. */
                cursor_deactivate();
                gsh_active_terminal->visible = 0u;
                gsh_active_terminal->minimized = 1u;
                if (window_id != 0u) {
                    display_server_hide_window(window_id);
                    display_server_focus_desktop();
                }
                fb_console_clear_bounds();
                vga_clear_bounds();
                shell_clear_exit_region();
                apps_container_one_set_app_count(1u);
                desktop_draw();
                cursor_show();
            } else {
                u32 window_id = gsh_window_id_for_terminal(gsh_active_terminal);
                gsh_active_terminal->visible = 0u;
                gsh_active_terminal->minimized = 1u;
                display_server_hide_window(window_id);
                shell_clear_exit_region();
                desktop_draw();
                cursor_show();
            }
            gsh_pointer_buttons = buttons;
            return;
        }
        if (control == TERMINAL_CONTROL_CLOSE) {
            gsh_launch_pressed = 1u;
        }
    }

    if (left_pressed && gsh_close_active_window(x, y)) {
        gsh_pointer_buttons = buttons;
        return;
    }

    if (left_pressed) {
        terminal_window_t *focused_terminal = gsh_terminal_for_window_id(
            display_server_get_active_window_id());
        if (focused_terminal && focused_terminal != gsh_active_terminal) {
            gsh_save_active_content();
            gsh_focus_terminal(focused_terminal,
                                display_server_get_active_window_id());
        }
    }

    terminal_window_t *terminal = gsh_active_terminal;
    int old_window_x = terminal->window.x;
    int old_window_y = terminal->window.y;
    u32 old_window_width = terminal->window.width;
    u32 old_window_height = terminal->window.height;

    window_handle_pointer(&terminal->window, x, y, buttons);

    const display_output_t *output = display_output_get();
    int max_x = (int)output->usable_width - (int)terminal->window.width + output->usable_x;
    int max_y = (int)output->usable_height - (int)terminal->window.height + output->usable_y;
    if (max_x < 0) max_x = 0;
    if (max_y < 0) max_y = 0;
    if (terminal->window.x < output->usable_x) terminal->window.x = output->usable_x;
    if (terminal->window.y < output->usable_y) terminal->window.y = output->usable_y;
    if (terminal->window.x > max_x) terminal->window.x = max_x;
    if (terminal->window.y > max_y) terminal->window.y = max_y;

    if (terminal->window.x != old_window_x || terminal->window.y != old_window_y) {
        cursor_deactivate();
        gsh_save_terminal_content(terminal);
        terminal_window_sync_layout(terminal);

        repaint_exposed_wallpaper(old_window_x, old_window_y,
                      old_window_width, old_window_height,
                      terminal->window.x, terminal->window.y);
        redraw_other_gsh_windows(terminal);

        if (gsh_server_window_id != 0u) {
            display_server_client_move_window(gsh_server_client_id,
                                              gsh_server_window_id,
                                              terminal->window.x,
                                              terminal->window.y);
        }

        terminal_window_set_bounds(terminal);
        shell_set_exit_region(terminal->window.x + (int)terminal->window.width - 16,
                      terminal->window.y + 2, 12, 12);

        gsh_draw_terminal_content(terminal);
        gsh_restore_terminal_cursor(terminal);
        cursor_show();
    }

    gsh_pointer_buttons = buttons;
}
