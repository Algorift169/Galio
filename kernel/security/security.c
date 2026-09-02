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

/* security.c - Security event handlers used to centralize fatal/soft security
 * responses. These wrappers make it easier to audit and change behavior later.
 */
#include "security/security.h"
#include "kprintf.h"
#include "common.h"

void security_warn(const char *msg) {
    kprintf("SECURITY WARNING: %s\n", msg);
}

void security_panic(const char *msg) {
    kprintf("SECURITY PANIC: %s\n", msg);
    panic(msg);
}
