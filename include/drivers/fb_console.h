#ifndef FB_CONSOLE_H
#define FB_CONSOLE_H

#include "common.h"

void fb_console_init(void);
u8 fb_console_active(void);
void fb_console_putc(char c);

#endif /* FB_CONSOLE_H */