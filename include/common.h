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

#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stddef.h>

/* Canonical fixed-width type aliases for the 64-bit kernel ABI */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef int64_t  i64;
typedef int32_t  i32;

/* Kernel utility functions */
void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void panic(const char *msg);
void assert_failed(const char *expr, const char *file, i32 line);
void stack_trace(void);

#define ASSERT(expr) ((expr) ? (void)0 : assert_failed(#expr, __FILE__, __LINE__))

/* Kernel status reporting */
void kernel_status(void);

#endif /* COMMON_H */
