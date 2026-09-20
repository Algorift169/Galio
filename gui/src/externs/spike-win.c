#include "spike.h"
#include "common.h"
#include "window.h"
#include "win-border.h"
#include "framebuffer.h"
#include "pit.h"
#include "process.h"
#include "keyboard.h"
#include "string.h"
#include "vga.h"
#include "mouse/cursor.h"
#include "mouse/mouse.h"
#include "display_output.h"
#include "display_wrapper.h"
#include "apps_container.h"
#include "panel.h"
#include "terminal_window.h"
#include "srver/client.h"
#include "srver/server.h"

#define SPIKE_WINDOW_WIDTH 420u
#define SPIKE_WINDOW_HEIGHT 220u
#define SPIKE_GRAPH_WIDTH 300u
#define SPIKE_GRAPH_HEIGHT 110u
#define SPIKE_SAMPLE_TICKS 100u
#define SPIKE_SAMPLE_COUNT 60u

static volatile u8 spike_launch_state;
static window_t *spike_render_window;
static const u8 *spike_render_samples;
static u32 spike_render_count;
static u32 spike_window_id;
static volatile u8 spike_restore_requested;
static volatile u8 spike_close_requested;
static volatile u8 spike_minimize_requested;

static void spike_draw_frame(const window_t *window) {
    u32 x;
    u32 y;
    u32 w;
    u32 h;
    u32 close_x;
    u32 minimize_x;
    u32 fullscreen_x;
    if (!window) return;

    x = (u32)window->x;
    y = (u32)window->y;
    w = window->width;
    h = window->height;
    fb_fill_rect(x, y, w, h, window->background);
    fb_draw_hline(x, y, w, window->border_color);
    fb_draw_hline(x, y + 14u, w, window->border_color);
    fb_draw_hline(x, y + h - 1u, w, window->border_color);

    close_x = x + w - 4u - 12u;
    minimize_x = close_x - 2u - 12u;
    fullscreen_x = minimize_x - 2u - 12u;
    fb_fill_rect(fullscreen_x, y + 2u, 12u, 12u, FB_COLOR(80u, 210u, 100u));
    fb_fill_rect(minimize_x, y + 2u, 12u, 12u, FB_COLOR(235u, 190u, 55u));
    fb_fill_rect(close_x, y + 2u, 12u, 12u, FB_COLOR(235u, 70u, 70u));

    if (window->name[0] != '\0') {
        for (u32 i = 0u; i < strlen(window->name) && i < 32u; i++) {
            fb_put_pixel(x + 8u + (i * 8u), y + 6u, window->title_color);
        }
    }
}

void spike_window_prepare_launch(void) {
    spike_launch_state = 0u;
    spike_close_requested = 0u;
    spike_minimize_requested = 0u;
}

u8 spike_window_launch_state(void) {
    return spike_launch_state;
}

void spike_window_restore(void) {
    if (spike_window_id != DISPLAY_SERVER_WINDOW_ID_NONE) {
        spike_restore_requested = 1u;
        display_server_show_window(spike_window_id);
        display_server_focus_window(spike_window_id);
        display_server_output_refresh();
    }
}

static u8 spike_sample_due(u32 now, u32 next_sample) {
    return (u32)(now - next_sample) >= SPIKE_SAMPLE_TICKS;
}

static void spike_draw_graph(window_t *window, const u8 *samples, u32 count) {
    int x0;
    int y0;
    int right;
    int bottom;
    u32 graph_width;
    u32 graph_height;
    u32 chart_x;
    u32 chart_y;
    u32 chart_w;
    u32 chart_h;
    u32 i;

    if (!window || !window->visible) return;

    x0 = window->x + 32;
    y0 = window->y + 32;
    right = window->x + (int)window->width - 18;
    bottom = window->y + (int)window->height - 18;
    graph_width = (u32)(right - x0);
    graph_height = (u32)(bottom - y0);
    chart_x = (u32)x0;
    chart_y = (u32)y0;
    chart_w = graph_width;
    chart_h = graph_height;

    fb_fill_rect(chart_x, chart_y, chart_w, chart_h, FB_COLOR(20u, 26u, 35u));
    fb_draw_rect(chart_x, chart_y, chart_w, chart_h, FB_COLOR(110u, 130u, 150u));
    panel_draw_text(window->x + 2, window->y + 38, "CPU%",
                    FB_COLOR(235u, 242u, 245u));
    panel_draw_text(window->x + 172, window->y + (int)window->height - 13,
                    "TIME", FB_COLOR(235u, 242u, 245u));

    for (i = 0u; i < chart_w; i += 18u) {
        fb_draw_vline(chart_x + i, chart_y, chart_h, FB_COLOR(38u, 46u, 58u));
    }
    for (i = 0u; i < chart_h; i += 18u) {
        fb_draw_hline(chart_x, chart_y + i, chart_w, FB_COLOR(38u, 46u, 58u));
    }

    if (count == 0u) {
        return;
    }

    for (i = 0u; i < count; i++) {
        u32 sample = samples[i];
        u32 bar_width = chart_w / SPIKE_SAMPLE_COUNT;
        u32 bar_height = ((u32)sample * chart_h + 99u) / 100u;
        if (bar_height < 3u) {
            bar_height = 3u;
        }
        u32 x = chart_x + i * bar_width + 1u;
        u32 y = chart_y + chart_h - bar_height;
        u32 w = bar_width > 2u ? bar_width - 2u : 1u;
        u32 color = sample >= 80u ? FB_COLOR(220u, 70u, 70u) :
                    (sample >= 50u ? FB_COLOR(220u, 180u, 60u) :
                     FB_COLOR(80u, 210u, 100u));

        if (x + w > chart_x + chart_w) {
            w = chart_x + chart_w - x;
        }
        if (w == 0u) {
            continue;
        }
        fb_fill_rect(x, y, w, bar_height, color);
        if (bar_height >= 3u && w >= 3u) {
            fb_draw_rect(x, y, w, bar_height, FB_COLOR(10u, 10u, 20u));
        }
    }

    {
        u32 value = samples[count - 1u];
        char label[16];
        int label_len = 0;
        label[0] = '\0';
        label_len = 0;
        while (value > 0u && label_len < 15) {
            label[label_len++] = (char)('0' + (value % 10u));
            value /= 10u;
        }
        if (label_len == 0) {
            label[label_len++] = '0';
        }
        for (int j = 0; j < label_len / 2; j++) {
            char tmp = label[j];
            label[j] = label[label_len - 1 - j];
            label[label_len - 1 - j] = tmp;
        }
        label[label_len] = '%';
        label[label_len + 1u] = '\0';

        fb_fill_rect(window->x + 12, window->y + 18, 72u, 12u, FB_COLOR(15u, 18u, 26u));
        for (u32 col = 0u; col < strlen(label) && col < 8u; col++) {
            u32 px = window->x + 18u + (col * 8u);
            fb_fill_rect(px, window->y + 20u, 7u, 10u, FB_COLOR(245u, 245u, 245u));
        }
        fb_fill_rect(window->x + 12, window->y + 18, 12u, 12u, FB_COLOR(80u, 210u, 100u));
    }
}

static u8 spike_window_control_at(const window_t *window, int x, int y) {
    int right;
    int top;
    int control_x;

    if (!window || !window->visible) return TERMINAL_CONTROL_NONE;
    right = window->x + (int)window->width - 4;
    top = window->y + 2;
    if (y < top || y >= top + 12) return TERMINAL_CONTROL_NONE;

    control_x = right - 12;
    if (x >= control_x && x < right) return TERMINAL_CONTROL_CLOSE;
    control_x -= (2 + 12);
    if (x >= control_x && x < control_x + 12) return TERMINAL_CONTROL_MINIMIZE;
    control_x -= (2 + 12);
    if (x >= control_x && x < control_x + 12) return TERMINAL_CONTROL_FULLSCREEN;
    return TERMINAL_CONTROL_NONE;
}

void spike_window_handle_pointer(int x, int y) {
    if (!spike_render_window || spike_window_id == DISPLAY_SERVER_WINDOW_ID_NONE ||
        !window_contains(spike_render_window, x, y)) {
        return;
    }

    display_server_focus_at_point(x, y);
    if (display_server_get_active_window_id() != spike_window_id) return;

    u8 control = spike_window_control_at(spike_render_window, x, y);
    if (control == TERMINAL_CONTROL_CLOSE) {
        spike_close_requested = 1u;
        return;
    }
    if (control == TERMINAL_CONTROL_MINIMIZE) {
        spike_minimize_requested = 1u;
        apps_container_one_set_spike_active(1u);
        display_server_hide_window(spike_window_id);
        display_server_focus_desktop();
        display_server_output_refresh();
    }
}

static void spike_window_redraw(window_t *window, const u8 *samples, u32 count) {
    u8 cursor_was_visible;

    if (!window) return;
    cursor_was_visible = cursor_is_visible();
    cursor_deactivate();
    spike_draw_frame(window);
    spike_draw_graph(window, samples, count);
    if (cursor_was_visible) cursor_show();
}

static void spike_window_update_graph(window_t *window, const u8 *samples, u32 count) {
    u8 cursor_was_visible;

    if (!window || !window->visible) return;
    if (display_server_get_active_window_id() == spike_window_id) {
        cursor_was_visible = cursor_is_visible();
        cursor_deactivate();
        spike_draw_graph(window, samples, count);
        if (cursor_was_visible) cursor_show();
        return;
    }

    display_server_output_refresh_region((u32)window->x, (u32)window->y,
                                         window->width, window->height);
}

static void spike_window_server_render(void) {
    if (spike_render_window) {
        spike_window_redraw(spike_render_window, spike_render_samples, spike_render_count);
    }
}

u8 spike_window_run(const char *args, const char *current_dir) {
    window_t window;
    u32 client_id;
    u32 window_id;
    u8 samples[SPIKE_SAMPLE_COUNT] = {0u};
    u32 count = 0u;
    u32 next_sample;
    u8 should_exit = 0u;
    u8 minimized = 0u;
    u8 fullscreen = 0u;
    int saved_x = 0;
    int saved_y = 0;
    u32 saved_w = 0u;
    u32 saved_h = 0u;
    int mouse_x = 0;
    int mouse_y = 0;
    u8 mouse_buttons = 0u;
    u8 last_mouse_buttons = 0u;
    (void)args;
    (void)current_dir;

    if (args) {
        while (*args == ' ' || *args == '\t') args++;
        if (strcmp(args, "help") == 0 || strcmp(args, "-h") == 0 || strcmp(args, "--help") == 0) {
            return 1u;
        }
        if (*args != '\0') {
            return 0u;
        }
    }

    client_id = display_server_client_connect();
    if (client_id == DISPLAY_SERVER_CLIENT_ID_NONE) {
        spike_launch_state = 2u;
        return 0u;
    }

    window_init(&window, "CPU Spike", FB_COLOR(18u, 24u, 32u), 180u, 90u,
                SPIKE_WINDOW_WIDTH, SPIKE_WINDOW_HEIGHT);
    window.border_color = FB_COLOR(120u, 150u, 170u);
    window.title_color = FB_COLOR(245u, 245u, 245u);
    window.visible = 1u;
    window_id = display_server_client_create_window(client_id, "CPU Spike", window.x, window.y,
                                                    window.width, window.height);
    if (window_id == DISPLAY_SERVER_WINDOW_ID_NONE) {
        spike_launch_state = 2u;
        return 0u;
    }
    spike_window_id = window_id;
    display_server_focus_window(window_id);
    spike_render_window = &window;
    spike_render_samples = samples;
    spike_render_count = count;
    display_server_client_set_window_render_callback(client_id, window_id,
                                                      spike_window_server_render);
    cursor_show();
    next_sample = pit_get_ticks();
    samples[count++] = process_get_cpu_usage();
    spike_render_count = count;
    spike_window_redraw(&window, samples, count);
    spike_launch_state = 1u;
    process_yield();

    while (!should_exit) {
        u32 now;
        u8 control;
        u8 left_pressed;
        u8 left_held;

        mouse_poll_position();
        mouse_get_position(&mouse_x, &mouse_y);
        mouse_buttons = mouse_get_buttons();
        left_pressed = (u8)((mouse_buttons & 0x01u) && !(last_mouse_buttons & 0x01u));
        left_held = (u8)(mouse_buttons & 0x01u);

        if (left_pressed) {
            display_server_focus_at_point(mouse_x, mouse_y);
        }

        if (display_server_get_active_window_id() == window_id) {
            control = spike_window_control_at(&window, mouse_x, mouse_y);

            if (left_pressed) {
                if (control == TERMINAL_CONTROL_CLOSE) {
                    should_exit = 1u;
                    last_mouse_buttons = mouse_buttons;
                    continue;
                }
                if (control == TERMINAL_CONTROL_MINIMIZE) {
                    minimized = 1u;
                    apps_container_one_set_spike_active(1u);
                    display_server_hide_window(window_id);
                    display_server_focus_desktop();
                    last_mouse_buttons = mouse_buttons;
                    continue;
                }
                if (control == TERMINAL_CONTROL_FULLSCREEN) {
                    const display_output_t *output = display_output_get();
                    if (!fullscreen) {
                        saved_x = window.x;
                        saved_y = window.y;
                        saved_w = window.width;
                        saved_h = window.height;
                        window.x = output->usable_x;
                        window.y = output->usable_y;
                        window.width = output->usable_width;
                        window.height = output->usable_height;
                        fullscreen = 1u;
                    } else {
                        window.x = saved_x;
                        window.y = saved_y;
                        window.width = saved_w;
                        window.height = saved_h;
                        fullscreen = 0u;
                    }
                    display_server_client_move_window(client_id, window_id,
                                                      window.x, window.y);
                    display_server_resize_window(window_id, window.width,
                                                 window.height);
                    spike_window_redraw(&window, samples, count);
                    last_mouse_buttons = mouse_buttons;
                    continue;
                }
            }

            if (left_held && control == TERMINAL_CONTROL_NONE &&
                (window.dragging ||
                 (window_contains(&window, mouse_x, mouse_y) &&
                  mouse_y >= window.y && mouse_y < window.y + 20))) {
                int old_x = window.x;
                int old_y = window.y;

                if (!window.dragging) {
                    window.drag_offset_x = mouse_x - window.x;
                    window.drag_offset_y = mouse_y - window.y;
                    window.dragging = 1u;
                }

                window.x = mouse_x - window.drag_offset_x;
                window.y = mouse_y - window.drag_offset_y;

                if (window.x != old_x || window.y != old_y) {
                    cursor_deactivate();
                    display_server_client_move_window(client_id, window_id,
                                                      window.x, window.y);
                    display_server_output_refresh_region(
                        (u32)(old_x < window.x ? old_x : window.x),
                        (u32)(old_y < window.y ? old_y : window.y),
                        (u32)((old_x + (int)window.width > window.x + (int)window.width ?
                               old_x + (int)window.width : window.x + (int)window.width) -
                              (old_x < window.x ? old_x : window.x)),
                        (u32)((old_y + (int)window.height > window.y + (int)window.height ?
                               old_y + (int)window.height : window.y + (int)window.height) -
                              (old_y < window.y ? old_y : window.y)));
                }
            } else if (!left_held) {
                window.dragging = 0u;
                window.drag_offset_x = 0;
                window.drag_offset_y = 0;
            }
        }

        if (spike_close_requested) {
            spike_close_requested = 0u;
            should_exit = 1u;
        }
        if (spike_minimize_requested) {
            spike_minimize_requested = 0u;
            minimized = 1u;
        }

        last_mouse_buttons = mouse_buttons;

        if (should_exit) {
            break;
        }

        if (!window.dragging) cursor_show();

        if (spike_restore_requested) {
            spike_restore_requested = 0u;
            minimized = 0u;
            window.visible = 1u;
            spike_window_redraw(&window, samples, count);
        }

        now = pit_get_ticks();
        if (spike_sample_due(now, next_sample)) {
            u32 sample = process_get_cpu_usage();
            next_sample = now;
            if (count < SPIKE_SAMPLE_COUNT) {
                samples[count++] = (u8)sample;
            } else {
                for (u32 i = 1u; i < SPIKE_SAMPLE_COUNT; i++) {
                    samples[i - 1u] = samples[i];
                }
                samples[SPIKE_SAMPLE_COUNT - 1u] = (u8)sample;
            }
            spike_render_count = count;
            spike_window_update_graph(&window, samples, count);
        }

        if (minimized) {
            process_yield();
            continue;
        }

        process_yield();
    }

    cursor_show();
    spike_render_window = (window_t *)0;
    spike_render_samples = (const u8 *)0;
    spike_render_count = 0u;
    display_server_client_set_window_render_callback(client_id, window_id, (void (*)(void))0);
    display_server_client_destroy_window(client_id, window_id);
    spike_window_id = DISPLAY_SERVER_WINDOW_ID_NONE;
    apps_container_one_set_spike_active(0u);
    return 1u;
}
