#include "clock.h"
#include "kernel_time.h"

static u32 last_second;
static u8 clock_started;

u8 clock_tick(void) {
    u32 current_second = kernel_time_get_seconds();

    if (!clock_started || current_second != last_second) {
        last_second = current_second;
        clock_started = 1u;
        return 1u;
    }
    return 0u;
}

void clock_format_datetime(char *date_string, char *time_string) {
    DateTime now = kernel_time_get_datetime();

    date_string[0] = (char)('0' + (now.year / 1000u) % 10u);
    date_string[1] = (char)('0' + (now.year / 100u) % 10u);
    date_string[2] = (char)('0' + (now.year / 10u) % 10u);
    date_string[3] = (char)('0' + now.year % 10u);
    date_string[4] = '-';
    date_string[5] = (char)('0' + now.month / 10u);
    date_string[6] = (char)('0' + now.month % 10u);
    date_string[7] = '-';
    date_string[8] = (char)('0' + now.day / 10u);
    date_string[9] = (char)('0' + now.day % 10u);
    date_string[10] = '\0';

    time_string[0] = (char)('0' + now.hour / 10u);
    time_string[1] = (char)('0' + now.hour % 10u);
    time_string[2] = ':';
    time_string[3] = (char)('0' + now.minute / 10u);
    time_string[4] = (char)('0' + now.minute % 10u);
    time_string[5] = ':';
    time_string[6] = (char)('0' + now.second / 10u);
    time_string[7] = (char)('0' + now.second % 10u);
    time_string[8] = '\0';
}