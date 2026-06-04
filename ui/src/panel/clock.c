#include "panel/clock.h"

static u32 utoa_digits(u32 num, char *buf, u32 min_width) {
    char temp[12];
    u32 len = 0;

    if (num == 0) {
        temp[len++] = '0';
    } else {
        while (num > 0 && len < sizeof(temp)) {
            temp[len++] = (char)('0' + (num % 10));
            num /= 10;
        }
    }

    u32 pad = (min_width > len) ? (min_width - len) : 0;
    for (u32 i = 0; i < pad; i++) {
        buf[i] = '0';
    }

    for (u32 i = 0; i < len; i++) {
        buf[pad + i] = temp[len - 1 - i];
    }

    return pad + len;
}

void panel_format_time(u32 seconds, char *time_str) {
    u32 hours = (seconds / 3600) % 24;
    u32 minutes = (seconds / 60) % 60;

    u32 pos = 0;
    pos += utoa_digits(hours, &time_str[pos], 2);
    time_str[pos++] = ':';
    pos += utoa_digits(minutes, &time_str[pos], 2);
    time_str[pos] = '\0';
}
