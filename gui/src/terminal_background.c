#include "terminal_background.h"
#include "background.h"
#include "framebuffer.h"
#include "fb_console.h"

void terminal_background_enter(void) {
    u32 color = FB_COLOR(64, 0, 16);
    /* Keep the terminal background fixed at the requested color while the shell
     * scrolls and redraws. */
    background_fill(color, 255, 0);
    fb_console_set_background(color);
}