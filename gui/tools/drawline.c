#include "drawline.h"
#include "framebuffer.h"

void gui_draw_line(int x0, int y0, int x1, int y1, u32 color) {
    if (x0 < 0 || y0 < 0 || x1 < 0 || y1 < 0) return;
    fb_draw_line((u32)x0, (u32)y0, (u32)x1, (u32)y1, color);
}

void gui_draw_border(int x, int y, u32 width, u32 height, u32 color) {
    if (width < 2u || height < 2u || x < 0 || y < 0) return;
    gui_draw_line(x, y, x + (int)width - 1, y, color);
    gui_draw_line(x, y, x, y + (int)height - 1, color);
    gui_draw_line(x + (int)width - 1, y,
                  x + (int)width - 1, y + (int)height - 1, color);
    gui_draw_line(x, y + (int)height - 1,
                  x + (int)width - 1, y + (int)height - 1, color);
}
