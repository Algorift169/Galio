#include "mouse/mouse.h"
#include "arch/x86/cpu.h"

#define DATA 0x60
#define STATUS 0x64
#define CMD 0x64
#define AUX 0x20
#define INBUF 0x02
#define OUTBUF 0x01

static int mouse_x = 512;
static int mouse_y = 384;
static u8 packet[4];
static u8 packet_index;
static u8 buttons;
static s8 scroll_delta;
static u64 sequence;

static void wait_input(void) { while (inb(STATUS) & INBUF) {} }
static void wait_output(void) { while (!(inb(STATUS) & OUTBUF)) {} }
static void send_aux(u8 value) {
    wait_input(); outb(CMD, 0xD4); wait_input(); outb(DATA, value); wait_output(); (void)inb(DATA);
}

void mouse_init(void) {
    wait_input(); outb(CMD, 0xA8);
    send_aux(0xF6); send_aux(0xF4);
    mouse_x = 512; mouse_y = 384; packet_index = 0; buttons = 0; scroll_delta = 0;
}

void mouse_poll_position(void) {
    for (u32 sample = 0; sample < 32u; sample++) {
        u8 status = inb(STATUS);
        if (!(status & OUTBUF) || !(status & AUX)) return;
        u8 value = inb(DATA);
        if (packet_index == 0u && !(value & 0x08u)) continue;
        packet[packet_index++] = value;
        if (packet_index < 4u) continue;
        packet_index = 0u;
        if (packet[0] & 0xC0u) continue;
        s8 dx = (s8)packet[1];
        s8 dy = (s8)packet[2];
        buttons = packet[0] & 0x07u;
        mouse_x += dx * 2;
        mouse_y -= dy * 2;
        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_x >= 1024) mouse_x = 1023;
        if (mouse_y >= 768) mouse_y = 767;
        scroll_delta = (s8)((s8)packet[3]);
        sequence++;
    }
}

void mouse_get_position(int *x, int *y) { if (x) *x = mouse_x; if (y) *y = mouse_y; }
u8 mouse_get_buttons(void) { return buttons; }
void mouse_flush_port(void) { while (inb(STATUS) & OUTBUF) (void)inb(DATA); }
s8 mouse_get_scroll_delta(void) { s8 value = scroll_delta; scroll_delta = 0; return value; }
u8 mouse_read_event(mouse_event_t *event) { (void)event; return 0u; }
void mouse_disable(void) { send_aux(0xF5); buttons = 0; }
void mouse_enable(void) { send_aux(0xF4); }
