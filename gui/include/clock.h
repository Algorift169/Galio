#ifndef GUI_CLOCK_H
#define GUI_CLOCK_H

#include "common.h"

void clock_format_datetime(char *date_string, char *time_string);
u8 clock_tick(void);

#endif /* GUI_CLOCK_H */