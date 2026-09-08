#include "editor.h"
#include "vga.h"
#include "string.h"
#include "vfs.h"
#include "keyboard.h"
#include "arch/x86/cpu.h"
#include "irq.h"

#define EDITOR_BUFFER_SIZE 4096u
#define EDITOR_TEXT_COLOR 0x0Fu
#define EDITOR_CURSOR_COLOR 0xF0u

typedef struct {
    char data[EDITOR_BUFFER_SIZE];
    u32 size;
    u32 cursor;
    u8 ctrl_down;
} editor_state_t;

#define EDITOR_PS2_STATUS 0x64
#define EDITOR_PS2_DATA   0x60

static u8 editor_shift_down;
static u8 editor_ctrl_down;
static u8 editor_alt_down;
static u8 editor_extended;

static const u8 keymap[] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=', '\b','\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n',0,'a','s',
    'd','f','g','h','j','k','l',';','\'','`',0,'\\','z','x','c','v',
    'b','n','m',',','.','/',0,'*',0,' ',0,0,0,0,0,0
};
static const u8 shifted_keymap[] = {
    0,27,'!','@','#','$','%','^','&','*','(',')','_','+', '\b','\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,'A','S',
    'D','F','G','H','J','K','L',':','"','~',0,'|','Z','X','C','V',
    'B','N','M','<','>','?',0,'*',0,' ',0,0,0,0,0,0
};

static u8 editor_translate(u8 raw) {
    if (raw < sizeof(keymap)) {
        return editor_shift_down ? shifted_keymap[raw] : keymap[raw];
    }
    /* Set-1 keypad and navigation keys that produce printable characters. */
    switch (raw) {
        case 0x47: return '7';
        case 0x48: return '8';
        case 0x49: return '9';
        case 0x4A: return '-';
        case 0x4B: return '4';
        case 0x4C: return '5';
        case 0x4D: return '6';
        case 0x4E: return '+';
        case 0x4F: return '1';
        case 0x50: return '2';
        case 0x51: return '3';
        case 0x52: return '0';
        case 0x53: return '.';
        default: return 0;
    }
}

static u8 editor_read_scancode(u8 *scancode, u8 *pressed, u8 *extended) {
    u8 status = inb(EDITOR_PS2_STATUS);
    if (!(status & 0x01u) || (status & 0x20u)) return 0;

    u8 data = inb(EDITOR_PS2_DATA);
    if (data == 0xE0u) {
        editor_extended = 1;
        return 0;
    }

    if (scancode) *scancode = data & 0x7Fu;
    if (pressed) *pressed = (data & 0x80u) == 0;
    if (extended) *extended = editor_extended;
    editor_extended = 0;

    switch (data & 0x7Fu) {
        case 0x2Au:
        case 0x36u:
            editor_shift_down = (data & 0x80u) == 0;
            break;
        case 0x1Du:
            editor_ctrl_down = (data & 0x80u) == 0;
            break;
        case 0x38u:
            editor_alt_down = (data & 0x80u) == 0;
            break;
        default:
            break;
    }
    return 1;
}

static void editor_put(int x, int y, char character, u8 color) {
    if (x >= 0 && y >= 0 && x < 128 && y < 48) vga_write_cell(x, y, character, color);
}

static void editor_text(int x, int y, const char *text, u8 color) {
    while (text && *text && x < 128) editor_put(x++, y, *text++, color);
}

static u32 editor_line_column(const editor_state_t *state, u32 *line) {
    u32 column = 0;
    *line = 0;
    for (u32 index = 0; index < state->cursor; index++) {
        if (state->data[index] == '\n') {
            (*line)++;
            column = 0;
        } else column++;
    }
    return column;
}

static void editor_draw_cursor(const editor_state_t *state) {
    u32 line;
    u32 column = editor_line_column(state, &line);
    if (line >= 44u || column >= 128u) return;

    /* Match GSH: a visible white cell with the underlying character in black. */
    vga_move_hardware_cursor((int)column, (int)(4u + line));
    vga_write_cell((int)column, (int)(4u + line), ' ', EDITOR_CURSOR_COLOR);
}

static void editor_draw(const editor_state_t *state, const char *path, const char *status) {
    int x = 0;
    int y = 4;

    vga_clear();
    editor_text(0, 0, "Ctrl+S Save | Ctrl+X Exit | Arrows Move", EDITOR_TEXT_COLOR);
    editor_text(0, 1, "File: ", EDITOR_TEXT_COLOR);
    editor_text(6, 1, path, EDITOR_TEXT_COLOR);
    if (status) editor_text(0, 2, status, EDITOR_TEXT_COLOR);

    for (u32 index = 0; index < state->size && y < 47; index++) {
        char character = state->data[index];
        if (character == '\n') {
            x = 0;
            y++;
        } else {
            editor_put(x++, y, character, EDITOR_TEXT_COLOR);
            if (x >= 128) {
                x = 0;
                y++;
            }
        }
    }
    editor_draw_cursor(state);
}

static u8 editor_save(const char *path, const editor_state_t *state) {
    u32 fd = vfs_open(path);
    if (fd == VFS_INVALID_FD) return 0;
    u32 written = vfs_write(fd, (const u8 *)state->data, state->size);
    vfs_close(fd);
    if (written != state->size) return 0;
    vfs_fsync();
    return 1;
}

static void editor_insert(editor_state_t *state, char character) {
    if (state->size >= EDITOR_BUFFER_SIZE - 1) return;
    for (u32 index = state->size; index > state->cursor; index--) state->data[index] = state->data[index - 1];
    state->data[state->cursor++] = character;
    state->size++;
}

static void editor_delete_left(editor_state_t *state) {
    if (state->cursor == 0) return;
    for (u32 index = state->cursor - 1; index < state->size - 1; index++) state->data[index] = state->data[index + 1];
    state->cursor--;
    state->size--;
}

u8 shell_editor(const char *filepath) {
    editor_state_t state = {0};
    u8 scancode;
    u8 pressed;
    u8 extended;
    vfs_entry_t *entry;

    if (!filepath || !*filepath) return 0;
    entry = vfs_find(filepath);
    if (entry && !entry->is_dir && entry->size) {
        state.size = entry->size < EDITOR_BUFFER_SIZE - 1 ? entry->size : EDITOR_BUFFER_SIZE - 1;
        vfs_read(filepath, (u8 *)state.data, state.size);
        state.cursor = state.size;
    }

    /* Editor owns PS/2 input exclusively while active. */
    irq_mask(1);
    keyboard_reset_state();
    editor_shift_down = 0;
    editor_ctrl_down = 0;
    editor_alt_down = 0;
    editor_extended = 0;
    enable_interrupts();
    vga_disable_hardware_cursor();
    editor_draw(&state, filepath, NULL);

    for (;;) {
        if (!editor_read_scancode(&scancode, &pressed, &extended)) continue;
        u8 raw = scancode & 0x7Fu;
        if (!pressed) continue;

        state.ctrl_down = editor_ctrl_down;
        if (editor_ctrl_down && raw == 0x1F) {
            editor_draw(&state, filepath, editor_save(filepath, &state) ? "Saved" : "Save failed");
            continue;
        }
        if (editor_ctrl_down && raw == 0x2D) break;
        if (editor_ctrl_down || editor_alt_down) continue;

        if (extended) {
            if (raw == 0x4B && state.cursor) state.cursor--;
            else if (raw == 0x4D && state.cursor < state.size) state.cursor++;
            else continue;
            editor_draw(&state, filepath, NULL);
            continue;
        }
            u8 character = editor_translate(raw);
        if (character == '\b') editor_delete_left(&state);
        else if (character >= 32 && character < 127) editor_insert(&state, (char)character);
        else if (character == '\n') editor_insert(&state, '\n');
        else continue;
        editor_draw(&state, filepath, NULL);
    }

    keyboard_reset_state();
    irq_unmask(1);
    vga_enable_hardware_cursor();
    vga_clear();
    return 1;
}
