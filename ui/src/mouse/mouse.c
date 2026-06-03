#include "mouse/mouse.h"
#include "arch/x86/cpu.h"
#include "kprintf.h"

#define PS2_DATA_PORT      0x60
#define PS2_STATUS_PORT    0x64
#define PS2_COMMAND_PORT   0x64

#define PS2_CMD_ENABLE_AUX 0xA8
#define PS2_CMD_WRITE_BYTE 0x60
#define PS2_CMD_READ_BYTE  0x20
#define PS2_CMD_SEND_TO_AUX 0xD4

#define PS2_MOUSE_DEFAULTS       0xF6
#define PS2_MOUSE_ENABLE_DATA    0xF4
#define PS2_MOUSE_SET_STREAM     0xEA
#define PS2_MOUSE_SET_SAMPLE_RATE 0xF3
#define PS2_MOUSE_GET_DEVICE_ID  0xF2

#define PS2_STATUS_OUTPUT_BUFFER 0x01
#define PS2_STATUS_INPUT_BUFFER  0x02

static int mouse_x = 40;
static int mouse_y = 12;
static u8 packet[4];
static u8 packet_index = 0;
static u8 packet_length = 3;

static void ps2_wait_input(void) {
    while (inb(PS2_STATUS_PORT) & PS2_STATUS_INPUT_BUFFER) {
        for (volatile int i = 0; i < 10; i++);
    }
}

static void ps2_wait_output(void) {
    while (!(inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_BUFFER)) {
        for (volatile int i = 0; i < 10; i++);
    }
}

static void mouse_write(u8 data) {
    ps2_wait_input();
    outb(PS2_COMMAND_PORT, PS2_CMD_SEND_TO_AUX);
    ps2_wait_input();
    outb(PS2_DATA_PORT, data);
    ps2_wait_output();
}

static u8 mouse_read(void) {
    ps2_wait_output();
    return inb(PS2_DATA_PORT);
}

static u8 mouse_write_command(u8 command) {
    mouse_write(command);
    return mouse_read();
}

static u8 mouse_send_data(u8 data) {
    mouse_write(data);
    return mouse_read();
}

static u8 mouse_set_sample_rate(u8 rate) {
    if (mouse_write_command(PS2_MOUSE_SET_SAMPLE_RATE) != 0xFA) {
        return 0;
    }
    return mouse_send_data(rate);
}

static u8 mouse_get_device_id(void) {
    if (mouse_write_command(PS2_MOUSE_GET_DEVICE_ID) != 0xFA) {
        return 0xFF;
    }
    return mouse_read();
}

static void update_mouse_state(s8 dx, s8 dy, u8 buttons) {
    mouse_x += dx;
    mouse_y -= dy;  /* Invert Y axis for natural movement */
    
    /* Clamp to screen bounds (80x25) */
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_x >= 80) mouse_x = 79;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_y >= 25) mouse_y = 24;
}

void mouse_init(void) {
    kprintf("Initializing mouse...\n");
    
    /* Enable auxiliary port */
    outb(PS2_COMMAND_PORT, PS2_CMD_ENABLE_AUX);
    for (volatile int i = 0; i < 1000; i++);

    /* Read current command byte */
    ps2_wait_input();
    outb(PS2_COMMAND_PORT, PS2_CMD_READ_BYTE);
    ps2_wait_output();
    u8 command_byte = inb(PS2_DATA_PORT);
    
    /* Enable mouse IRQ and disable translation */
    command_byte |= 0x03;   /* Enable keyboard and mouse IRQs */
    command_byte &= ~0x10;  /* Disable translation */
    
    /* Write back command byte */
    ps2_wait_input();
    outb(PS2_COMMAND_PORT, PS2_CMD_WRITE_BYTE);
    ps2_wait_input();
    outb(PS2_DATA_PORT, command_byte);
    
    /* Reset mouse to defaults */
    mouse_write(PS2_MOUSE_DEFAULTS);
    mouse_read();
    
    /* Enable data reporting */
    mouse_write(PS2_MOUSE_ENABLE_DATA);
    mouse_read();
    
    /* Set stream mode */
    mouse_write(PS2_MOUSE_SET_STREAM);
    mouse_read();
    
    /* Try to enable scroll wheel (4-byte packets) */
    if (mouse_set_sample_rate(200) == 0xFA &&
        mouse_set_sample_rate(100) == 0xFA &&
        mouse_set_sample_rate(80) == 0xFA) {
        u8 id = mouse_get_device_id();
        if (id == 3) {
            packet_length = 4;
            kprintf("Mouse: Scroll wheel detected (4-byte packets)\n");
        }
    }
    
    mouse_x = 40;
    mouse_y = 12;
    packet_index = 0;
    
    kprintf("Mouse initialized (packet length: %u)\n", packet_length);
}

void mouse_poll_position(void) {
    u8 status = inb(PS2_STATUS_PORT);
    
    /* Check if data is available */
    if (!(status & PS2_STATUS_OUTPUT_BUFFER)) {
        return;
    }
    
    u8 data = inb(PS2_DATA_PORT);
    
    /* Start of new packet: bit 3 must be set */
    if (packet_index == 0) {
        if (!(data & 0x08)) {
            return;
        }
    }
    
    packet[packet_index++] = data;
    
    if (packet_index < packet_length) {
        return;
    }
    
    /* Reset for next packet */
    packet_index = 0;
    
    /* Check for overflow or invalid packet */
    if (packet[0] & 0xC0) {
        return;
    }
    
    s8 dx = (s8)packet[1];
    s8 dy = (s8)packet[2];
    u8 buttons = packet[0] & 0x07;
    
    update_mouse_state(dx, dy, buttons);
}

void mouse_get_position(int *x, int *y) {
    if (x) *x = mouse_x;
    if (y) *y = mouse_y;
}