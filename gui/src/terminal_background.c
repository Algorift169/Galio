#include "terminal_background.h"
#include "background.h"
#include "framebuffer.h"
#include "fb_console.h"

void terminal_background_enter(void) {
    u32 color = FB_COLOR(64, 0, 16);
    background_fill(color, 255, 1000);
    fb_console_set_background(color);
}