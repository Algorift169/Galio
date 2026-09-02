/*
 * Galio Kernel
 *
 * Copyright (C) 2026 Israfil [Your Legal Name]
 *
 * This file is part of Galio.
 *
 * Galio is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * Galio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Galio. If not, see <https://www.gnu.org/licenses/>.
 */

/* serial.c - COM port serial driver for debugging */
#include "serial.h"
#include "cpu.h"

#define COM1_PORT 0x3F8

/* UART Register Offsets */
#define UART_DATA          0
#define UART_INT_ENABLE    1
#define UART_FIFO_CTRL     2
#define UART_LINE_CTRL     3
#define UART_MODEM_CTRL    4
#define UART_LINE_STATUS   5
#define UART_MODEM_STATUS  6
#define UART_SCRATCH       7

/* Line Control bits */
#define UART_LC_8BITS      0x03
#define UART_LC_STOP1      0x00
#define UART_LC_PARITY_OFF 0x00
#define UART_LC_DLAB       0x80

/* Line Status bits */
#define UART_LS_EMPTY_THR  0x20

void serial_init(void) {
    /* Disable all interrupts */
    outb(COM1_PORT + UART_INT_ENABLE, 0x00);

    /* Set baud rate to 115200 */
    outb(COM1_PORT + UART_LINE_CTRL, UART_LC_DLAB);
    outb(COM1_PORT + UART_DATA, 0x01);         /* Divisor latch low byte */
    outb(COM1_PORT + UART_INT_ENABLE, 0x00);   /* Divisor latch high byte */

    /* 8 data bits, 1 stop bit, no parity */
    outb(COM1_PORT + UART_LINE_CTRL, UART_LC_8BITS | UART_LC_STOP1 | UART_LC_PARITY_OFF);

    /* Enable FIFO with 14-byte threshold */
    outb(COM1_PORT + UART_FIFO_CTRL, 0xC7);

    /* Set RTS and DTR */
    outb(COM1_PORT + UART_MODEM_CTRL, 0x0B);
}

void serial_putc(char c) {
    /* Non-blocking: only write if TX FIFO has space */
    if ((inb(COM1_PORT + UART_LINE_STATUS) & UART_LS_EMPTY_THR)) {
        outb(COM1_PORT + UART_DATA, (u8)c);
    }
}

void serial_puts(const char *s) {
    while (*s) {
        if (*s == '\n') {
            serial_putc('\r');
        }
        serial_putc(*s++);
    }
}