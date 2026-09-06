/*
 * Galio Kernel
 *
 * Copyright (C) 2026 S.M Israfil
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

#include "string.h"
#include <stddef.h>   // <-- add this here too

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n > 0 && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return (unsigned char)*s1 - (unsigned char)*s2;
}

size_t strlen(const char *s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while (*src) {
        *d++ = *src++;
    }
    *d = '\0';
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    char *d = dest;
    while (n > 0 && *src) {
        *d++ = *src++;
        n--;
    }
    while (n > 0) {
        *d++ = 0;
        n--;
    }
    return dest;
}

char *strncat(char *dest, const char *src, size_t n) {
    char *d = dest;
    while (*d) d++;

    while (n > 0 && *src) {
        *d++ = *src++;
        n--;
    }
    *d = 0;
    return dest;
}

char *strcat(char *dest, const char *src) {
    char *end = dest + strlen(dest);
    while (*src) *end++ = *src++;
    *end = '\0';
    return dest;
}

char *strchr(const char *text, int character) {
    while (*text) {
        if (*text == (char)character) return (char *)text;
        text++;
    }
    return character == '\0' ? (char *)text : NULL;
}

char *strstr(const char *text, const char *needle) {
    size_t needle_length;
    if (*needle == '\0') return (char *)text;
    needle_length = strlen(needle);
    while (*text) {
        if (strncmp(text, needle, needle_length) == 0) return (char *)text;
        text++;
    }
    return NULL;
}