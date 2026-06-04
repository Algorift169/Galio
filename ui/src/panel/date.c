#include "panel/date.h"

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

void panel_format_date(const DateTime *dt, char *date_str) {
    if (!dt || !date_str) {
        date_str[0] = '\0';
        return;
    }

    u32 pos = 0;
    pos += utoa_digits(dt->day, &date_str[pos], 2);
    date_str[pos++] = '/';
    pos += utoa_digits(dt->month, &date_str[pos], 2);
    date_str[pos++] = '/';
    pos += utoa_digits(dt->year, &date_str[pos], 4);
    date_str[pos] = '\0';
}
