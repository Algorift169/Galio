#ifndef SERIAL_H
#define SERIAL_H

#include "common.h"

void serial_init(void);
void serial_putc(char c);
void serial_puts(const char *s);

#endif /* SERIAL_H */
