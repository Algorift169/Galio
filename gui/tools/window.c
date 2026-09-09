#include "window.h"
#include "framebuffer.h"
#include "win-border.h"

void window_init(window_t *window,
                 const char *name,
                 u32 background,
                 u32 x,
                 u32 y,
                 u32 width,
                 u32 height)
{
    if (!window) return;
    if (name) {
        u32 len = 0;
        while (name[len] && len < sizeof(window->name) - 1u) {
            window->name[len] = name[len];
            len++;
        }
        window->name[len] = '\0';
    } else {
        window->name[0] = '\0';
    }

    window->background = background;
    window->x = (int)x;
    window->y = (int)y;
    window->width = width;
    window->height = height;
    window->border_color = 0x00D0D0D0u;
    window->title_color = 0x00FFFFFFu;
    window->draggable = 1u;
    window->resizeable = 1u;
    window->visible = 1u;
    window->closed = 0u;
    window->drag_offset_x = 0;
    window->drag_offset_y = 0;
    window->last_mouse_x = -1;
    window->last_mouse_y = -1;
}

void window_draw(const window_t *window) {
    if (!window || !window->visible || window->closed) return;
    win_border_draw(window, window->border_color, window->background);
}

void window_set_position(window_t *window, int x, int y) {
    if (!window) return;
    window->x = x;
    window->y = y;
}

void window_set_size(window_t *window, u32 width, u32 height) {
    if (!window) return;
    if (width < 80u) width = 80u;
    if (height < 60u) height = 60u;
    window->width = width;
    window->height = height;
}

void window_close(window_t *window) {
    if (!window) return;
    window->closed = 1u;
    window->visible = 0u;
}

void window_set_visible(window_t *window, u8 visible) {
    if (!window) return;
    window->visible = visible ? 1u : 0u;
}

u8 window_contains(const window_t *window, int x, int y) {
    if (!window || !window->visible || window->closed) return 0u;
    return (x >= window->x && x < (int)(window->x + (int)window->width) &&
            y >= window->y && y < (int)(window->y + (int)window->height));
}

void window_handle_pointer(window_t *window, int mouse_x, int mouse_y, u8 buttons) {
    if (!window || !window->visible || window->closed) return;

    if (buttons & 0x01u) {
        if (window->last_mouse_x >= 0 && window->last_mouse_y >= 0) {
            int dx = mouse_x - window->last_mouse_x;
            int dy = mouse_y - window->last_mouse_y;
            if (window->drag_offset_x != 0 || window->drag_offset_y != 0) {
                window->x += dx;
                window->y += dy;
            }
        }
        if (mouse_x >= (int)(window->x + window->width - 18u) &&
            mouse_y >= (int)(window->y + window->height - 18u) &&
            window->resizeable) {
            u32 new_width = (u32)(mouse_x - window->x + 18u);
            u32 new_height = (u32)(mouse_y - window->y + 18u);
            if (new_width > 80u) window->width = new_width;
            if (new_height > 60u) window->height = new_height;
        }
    }

    if (window->drag_offset_x == 0 && window->drag_offset_y == 0 &&
        mouse_x >= window->x && mouse_x < (int)(window->x + (int)window->width) &&
        mouse_y >= window->y && mouse_y < (int)(window->y + 20)) {
        window->drag_offset_x = mouse_x - window->x;
        window->drag_offset_y = mouse_y - window->y;
    }

    if (!(buttons & 0x01u)) {
        window->drag_offset_x = 0;
        window->drag_offset_y = 0;
    }

    window->last_mouse_x = mouse_x;
    window->last_mouse_y = mouse_y;
}
