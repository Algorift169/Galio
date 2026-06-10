#include "buttons/gsh.h"
#include "vga.h"
#include "shell.h"
#include "display/display.h"
#include "mouse/mouse.h"
#include "panel/panel.h"
#include "keyboard.h"

/* GSH button properties */
#define GSH_BUTTON_WIDTH 6   /* "[GSH]" = 5 chars + space */
#define GSH_BUTTON_HEIGHT 1
#define GSH_BUTTON_BG_COLOR 0x9F   /* Blue background (0x9) with white text (0xF) */
#define GSH_BUTTON_TEXT_COLOR 0x9F /* White text on blue background */

static int gsh_button_x = 9;
static int gsh_button_y = 0;
static int gsh_is_hovered = 0;
static u8 gsh_shell_active = 0;

void gsh_button_init(void) {
    /* Initialize GSH button */
    gsh_is_hovered = 0;
}

void gsh_button_draw(int x, int y) {
    gsh_button_x = x;
    gsh_button_y = y;
    
    /* Draw button with blue background */
    unsigned char bg_color = GSH_BUTTON_BG_COLOR;
    unsigned char text_color = GSH_BUTTON_TEXT_COLOR;
    
    if (gsh_is_hovered) {
        /* Bright cyan when hovered */
        bg_color = 0xBF;  /* Bright cyan background (0xB) with white text (0xF) */
    }
    
    /* Draw "[GSH]" button */
    vga_draw_button(x, y, GSH_BUTTON_WIDTH, GSH_BUTTON_HEIGHT, "[GSH]", text_color, bg_color);
}

void gsh_button_click(void) {
    if (gsh_shell_active) {
        return;
    }

    gsh_shell_active = 1;

    /* Launch shell when GSH button is clicked */
    display_enter_shell_mode();
    mouse_flush_port();
    keyboard_reset_state();
    shell_run();
    mouse_flush_port();
    keyboard_reset_state();
    panel_set_enabled(1);
    display_enter_userland_mode();
    gsh_shell_active = 0;
}

u8 gsh_button_contains(int x, int y) {
    return (x >= gsh_button_x && x < gsh_button_x + GSH_BUTTON_WIDTH &&
            y == gsh_button_y);
}

void gsh_button_get_size(int *width, int *height) {
    if (width) *width = GSH_BUTTON_WIDTH;
    if (height) *height = GSH_BUTTON_HEIGHT;
}

/* Mouse hover handler */
void gsh_button_set_hovered(u8 hovered) {
    gsh_is_hovered = hovered;
}
