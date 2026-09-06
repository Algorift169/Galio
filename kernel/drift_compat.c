/* SPDX-License-Identifier: AGPL-3.0-only */
#include <stdarg.h>
#include <stddef.h>

#include "kprintf.h"
#include "string.h"

int drift_errno;
int drift_isspace(int character);
int drift_isalpha(int character);

static unsigned short drift_ctype_table[384];
static const unsigned short *drift_ctype_table_pointer = drift_ctype_table + 128;
static int drift_ctype_initialized;

const unsigned short **__ctype_b_loc(void)
{
    if (!drift_ctype_initialized) {
        for (int character = 0; character < 256; ++character) {
            unsigned short flags = 0;
            if (drift_isspace(character)) flags |= 0x2000;
            if (drift_isalpha(character)) flags |= 0x0100;
            if (character >= '0' && character <= '9') flags |= 0x0800;
            drift_ctype_table[128 + character] = flags;
        }
        drift_ctype_initialized = 1;
    }
    return &drift_ctype_table_pointer;
}

int *__errno_location(void) { return &drift_errno; }

long drift_strtod(const char *text, char **end)
{
    long value = 0;
    const char *cursor = text;
    int sign = 1;
    if (*cursor == '-') { sign = -1; cursor++; }
    while (*cursor >= '0' && *cursor <= '9') value = value * 10 + (*cursor++ - '0');
    if (*cursor == '.') {
        cursor++;
        while (*cursor >= '0' && *cursor <= '9') cursor++;
    }
    if (end != NULL) *end = (char *)cursor;
    return value * sign;
}

long drift_strtol(const char *text, char **end, int base)
{
    long value = 0;
    const char *cursor = text;
    int sign = 1;
    (void)base;
    if (*cursor == '-') { sign = -1; cursor++; }
    while (*cursor >= '0' && *cursor <= '9') value = value * 10 + (*cursor++ - '0');
    if (end != NULL) *end = (char *)cursor;
    return value * sign;
}

int drift_isspace(int character) { return character == ' ' || character == '\t' || character == '\n' || character == '\r'; }
int drift_isalpha(int character) { return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z'); }
int drift_isalnum(int character) { return drift_isalpha(character) || (character >= '0' && character <= '9'); }
long drift_floor(long value) { return value; }
long drift_fmod(long left, long right) { return right == 0 ? 0 : left % right; }

int drift_snprintf(char *buffer, size_t size, const char *format, ...)
{
    va_list args;
    size_t used = 0;
    const char *cursor = format;
    if (size == 0) return 0;
    va_start(args, format);
    while (*cursor && used + 1 < size) {
        if (*cursor != '%') { buffer[used++] = *cursor++; continue; }
        cursor++;
        if (*cursor == 's') {
            const char *text = va_arg(args, const char *);
            while (*text && used + 1 < size) buffer[used++] = *text++;
        } else if (*cursor == 'l' && cursor[1] == 'd') {
            long value = va_arg(args, long);
            char number[32];
            int length = 0;
            if (value < 0) { number[length++] = '-'; value = -value; }
            do { number[length++] = (char)('0' + value % 10); value /= 10; } while (value && length < 31);
            while (length > 0 && used + 1 < size) buffer[used++] = number[--length];
            cursor++;
        } else if (*cursor == 'g') {
            long value = va_arg(args, long);
            buffer[used++] = (char)('0' + (value < 0 ? 0 : value % 10));
        } else {
            buffer[used++] = *cursor;
        }
        cursor++;
    }
    buffer[used] = 0;
    va_end(args);
    return (int)used;
}
