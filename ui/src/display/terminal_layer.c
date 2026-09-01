#include "display/terminal_layer.h"
#include "vga.h"

#define TERMINAL_WIDTH 80
#define TERMINAL_HEIGHT 25
#define TERMINAL_BACKGROUND 0x00
#define TERMINAL_TEXT 0x0F

void terminal_layer_enter(void) {
    vga_clear_bounds();
    vga_clear();

    /* Paint every cell so no previous UI attributes remain behind shell input. */
    for (int y = 0; y < TERMINAL_HEIGHT; y++) {
        for (int x = 0; x < TERMINAL_WIDTH; x++) {
            vga_write_cell(x, y, ' ', TERMINAL_BACKGROUND);
        }
    }

    vga_set_color(TERMINAL_TEXT);
    vga_update_cursor();
}
