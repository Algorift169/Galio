#include "button.h"
#include "framebuffer.h"

void button_init(button_t *button,
                 const char *name,
                 u32 x,
                 u32 y,
                 u32 width,
                 u32 height,
                 u32 bg_color,
                 u32 text_color)
{
    if (!button) return;
    button->x = (int)x;
    button->y = (int)y;
    button->width = width;
    button->height = height;
    button->background = bg_color;
    button->text_color = text_color;
    button->visible = 1u;
    button->enabled = 1u;
    button->name[0] = '\0';
    if (name) {
        u32 idx = 0u;
        while (name[idx] && idx < sizeof(button->name) - 1u) {
            button->name[idx] = name[idx];
            idx++;
        }
        button->name[idx] = '\0';
    }
}

void button_draw(const button_t *button) {
    if (!button || !button->visible) return;
    fb_fill_rect((u32)button->x, (u32)button->y, button->width, button->height, button->background);
    fb_draw_rect((u32)button->x, (u32)button->y, button->width, button->height, 0x00D0D0D0u);
}

u8 button_contains(const button_t *button, int x, int y) {
    if (!button || !button->visible || !button->enabled) return 0u;
    return (x >= button->x && x < (int)(button->x + (int)button->width) &&
            y >= button->y && y < (int)(button->y + (int)button->height));
}
