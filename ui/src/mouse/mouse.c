#include "mouse/mouse.h"
#include "arch/x86/cpu.h"

#define PS2_DATA_PORT      0x60
#define PS2_STATUS_PORT    0x64
#define PS2_COMMAND_PORT   0x64

#define PS2_CMD_ENABLE_AUX 0xA8
#define PS2_CMD_DISABLE_AUX 0xA7
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
#define PS2_STATUS_AUX_OUTPUT    0x20

static int mouse_x = 40;
static int mouse_y = 12;
static int mouse_residual_x = 0;
static int mouse_residual_y = 0;
static u8 packet[4];
static u8 packet_index = 0;
static u8 packet_length = 3;
static u8 mouse_buttons = 0;
static s8 mouse_scroll_delta = 0;
#define MOUSE_EVENT_QUEUE_SIZE 128
static mouse_event_t mouse_events[MOUSE_EVENT_QUEUE_SIZE];
static u32 mouse_event_head = 0;
static u32 mouse_event_tail = 0;
static u64 mouse_event_sequence = 0;

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
    (void)buttons;
    mouse_residual_x += dx;
    mouse_residual_y += dy;

    int scaled_dx = mouse_residual_x / 10;
    int scaled_dy = mouse_residual_y / 10;

    mouse_residual_x -= scaled_dx * 10;
    mouse_residual_y -= scaled_dy * 10;

    mouse_x += scaled_dx;
    mouse_y -= scaled_dy;  /* Invert Y axis for natural movement */
    
    /* Clamp to screen bounds (80x25) */
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_x >= 80) mouse_x = 79;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_y >= 25) mouse_y = 24;
}

static void mouse_enqueue_event(s8 dx, s8 dy, u8 buttons, s8 wheel) {
    u32 next = (mouse_event_head + 1) % MOUSE_EVENT_QUEUE_SIZE;
    if (next == mouse_event_tail) {
        mouse_event_tail = (mouse_event_tail + 1) % MOUSE_EVENT_QUEUE_SIZE;
    }
    mouse_event_t *event = &mouse_events[mouse_event_head];
    event->dx = dx;
    event->dy = dy;
    event->buttons = buttons;
    event->flags = 0;
    if (dx || dy) event->flags |= MOUSE_EVENT_MOVE;
    if (buttons != mouse_buttons) event->flags |= MOUSE_EVENT_BUTTON;
    if (wheel) event->flags |= MOUSE_EVENT_WHEEL;
    event->wheel = wheel;
    event->timestamp = mouse_event_sequence++;
    mouse_event_head = next;
}

void mouse_init(void) {
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

    /* Always enable the 4-byte packet format that carries wheel delta. This is a kernel-level
       guarantee for scroll support across all apps, regardless of the specific device ID. */
    packet_length = 4;
    (void)mouse_set_sample_rate(200);
    (void)mouse_set_sample_rate(100);
    (void)mouse_set_sample_rate(80);
    (void)mouse_get_device_id();
    
    mouse_x = 40;
    mouse_y = 12;
    packet_index = 0;
    mouse_event_head = 0;
    mouse_event_tail = 0;
}

void mouse_poll_position(void) {
    /* Drain a bounded batch so queued four-byte wheel packets do not take
       several delayed shell iterations to reach the scroll handler. */
    for (u32 sample = 0; sample < 32; sample++) {
        u8 status = inb(PS2_STATUS_PORT);

        /* Only read from the aux/mouse output buffer. Keyboard data must not be drained here. */
        if (!(status & PS2_STATUS_AUX_OUTPUT)) {
            return;
        }

        u8 data = inb(PS2_DATA_PORT);
    
    /* Start of new packet: bit 3 must be set */
    if (packet_index == 0) {
        if (!(data & 0x08)) {
            continue;
        }
    }
    
    packet[packet_index++] = data;
    
        if (packet_index < packet_length) {
            continue;
        }
    
    /* Reset for next packet */
    packet_index = 0;
    
    /* Check for overflow or invalid packet */
        if (packet[0] & 0xC0) {
            continue;
    }
    
    s8 dx = (s8)packet[1];
    s8 dy = (s8)packet[2];
    u8 buttons = packet[0] & 0x07;

    if (packet_length == 4) {
        /* Wheel packets are always 4 bytes in the kernel mouse layer. Decode the signed
           4th-byte delta and expose it via the generic scroll API for all consumers. */
        s8 wheel = (s8)packet[3];
        if (wheel != 0) {
            int accumulated = (int)mouse_scroll_delta + (int)wheel;
            if (accumulated > 127) accumulated = 127;
            if (accumulated < -127) accumulated = -127;
            mouse_scroll_delta = (s8)accumulated;
        }
        mouse_enqueue_event(dx, dy, buttons, wheel);
    } else {
        mouse_enqueue_event(dx, dy, buttons, 0);
    }
    
        mouse_buttons = buttons;
        update_mouse_state(dx, dy, buttons);
    }
}

u8 mouse_get_buttons(void) {
    return mouse_buttons;
}

void mouse_flush_port(void) {
    while (inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_BUFFER) {
        (void)inb(PS2_DATA_PORT);
    }
}

void mouse_disable(void) {
    /* Disable mouse data reporting and mask the mouse IRQ. */
    mouse_send_data(0xF5);  /* Disable data reporting */
    mouse_buttons = 0;

    mouse_flush_port();
    ps2_wait_input();
    outb(PS2_COMMAND_PORT, PS2_CMD_DISABLE_AUX);

    mouse_flush_port();
    ps2_wait_input();
    outb(PS2_COMMAND_PORT, PS2_CMD_READ_BYTE);
    ps2_wait_output();
    u8 command_byte = inb(PS2_DATA_PORT);
    
    command_byte &= ~0x02;  /* Disable auxiliary (mouse) IRQ */
    command_byte |= 0x01;   /* Ensure keyboard IRQ remains enabled */

    ps2_wait_input();
    outb(PS2_COMMAND_PORT, PS2_CMD_WRITE_BYTE);
    ps2_wait_input();
    outb(PS2_DATA_PORT, command_byte);

    mouse_flush_port();
}

void mouse_enable(void) {
    /* Re-enable the auxiliary PS/2 port */
    outb(PS2_COMMAND_PORT, PS2_CMD_ENABLE_AUX);
    for (volatile int i = 0; i < 1000; i++);

    /* Read current command byte */
    mouse_flush_port();
    ps2_wait_input();
    outb(PS2_COMMAND_PORT, PS2_CMD_READ_BYTE);
    ps2_wait_output();
    u8 command_byte = inb(PS2_DATA_PORT);

    /* Enable mouse IRQ (bit 1) and keyboard IRQ (bit 0) */
    command_byte |= 0x03;

    /* Write back command byte */
    ps2_wait_input();
    outb(PS2_COMMAND_PORT, PS2_CMD_WRITE_BYTE);
    ps2_wait_input();
    outb(PS2_DATA_PORT, command_byte);

    /* Re-enable data reporting */
    mouse_write(PS2_MOUSE_ENABLE_DATA);
    mouse_read();

    /* Re-enable scroll wheel (4-byte packet mode) via the magic sample rate sequence */
    packet_length = 4;
    (void)mouse_set_sample_rate(200);
    (void)mouse_set_sample_rate(100);
    (void)mouse_set_sample_rate(80);
    (void)mouse_get_device_id();

    /* Reset packet state */
    packet_index = 0;
    mouse_scroll_delta = 0;
    mouse_event_head = 0;
    mouse_event_tail = 0;

    /* Flush any stale bytes */
    mouse_flush_port();
}

void mouse_get_position(int *x, int *y) {
    if (x) *x = mouse_x;
    if (y) *y = mouse_y;
}

s8 mouse_get_scroll_delta(void) {
    s8 delta = mouse_scroll_delta;
    mouse_scroll_delta = 0;
    return delta;
}

u8 mouse_read_event(mouse_event_t *event) {
    if (!event || mouse_event_head == mouse_event_tail) return 0;
    *event = mouse_events[mouse_event_tail];
    mouse_event_tail = (mouse_event_tail + 1) % MOUSE_EVENT_QUEUE_SIZE;
    return 1;
}