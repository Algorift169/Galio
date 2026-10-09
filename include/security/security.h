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

/* security.h - Kernel security helpers */
#ifndef SECURITY_H
#define SECURITY_H

#include "common.h"
#include "process/process.h"

#define SECURITY_CAP_KILL          (1ULL << 0)
#define SECURITY_CAP_FOWNER        (1ULL << 1)
#define SECURITY_CAP_DAC_OVERRIDE  (1ULL << 2)
#define SECURITY_CAP_ALL (SECURITY_CAP_KILL | SECURITY_CAP_FOWNER | SECURITY_CAP_DAC_OVERRIDE)

#define SECURITY_AUDIT_CAPACITY 64u
#define SECURITY_AUDIT_TARGET_MAX 64u

typedef enum {
	SECURITY_AUDIT_SIGNAL_DENIED = 1,
	SECURITY_AUDIT_CHMOD_DENIED,
	SECURITY_AUDIT_FILE_DENIED
} security_audit_operation_t;

typedef struct {
	u64 sequence;
	u32 tick;
	u32 pid;
	u32 uid;
	u32 operation;
	u32 target_id;
	i32 result;
	char target[SECURITY_AUDIT_TARGET_MAX];
} security_audit_event_t;

void security_warn(const char *msg);
void security_panic(const char *msg);
u8 security_capable(const process_t *process, u64 capability);
u8 security_may_signal(const process_t *actor, const process_t *target);
u8 security_may_chmod(const process_t *actor, const char *path);
u8 security_check_file_access(const process_t *actor, const char *path, u32 access);
u8 security_check_parent_access(const process_t *actor, const char *path, u32 access);
u64 security_audit_count(void);
u8 security_audit_read(u64 sequence, security_audit_event_t *event);

#endif /* SECURITY_H */
