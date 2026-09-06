/* SPDX-License-Identifier: AGPL-3.0-only */
#ifndef DRIFT_KERNEL_COMPAT_H
#define DRIFT_KERNEL_COMPAT_H

#include <stddef.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "mm/heap.h"

void kprintf(const char *format, ...);

typedef long drift_float_t;
#define DRIFT_KERNEL 1
#define double drift_float_t
#define malloc kmalloc
#define calloc kcalloc
#define realloc krealloc
#define free kfree
#define strtod drift_strtod
#define strtol drift_strtol
#define floor drift_floor
#define fmod drift_fmod
#define isspace drift_isspace
#define isalpha drift_isalpha
#define isalnum drift_isalnum
#define snprintf drift_snprintf
#define printf kprintf
#define fprintf(stream, ...) kprintf(__VA_ARGS__)
#undef INFINITY
#define INFINITY 0x7fffffffffffffffL

long drift_strtod(const char *text, char **end);
long drift_strtol(const char *text, char **end, int base);
long drift_floor(long value);
long drift_fmod(long left, long right);
int drift_isspace(int character);
int drift_isalpha(int character);
int drift_isalnum(int character);
int drift_snprintf(char *buffer, size_t size, const char *format, ...);
extern int drift_errno;
#define errno drift_errno

#endif
