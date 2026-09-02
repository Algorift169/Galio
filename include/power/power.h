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

#ifndef POWER_H
#define POWER_H

#include "common.h"
#include <stdbool.h>

#define PM_SUSPEND_ON        0
#define PM_SUSPEND_MIN       0
#define PM_SUSPEND_TO_IDLE   1
#define PM_SUSPEND_STANDBY   2
#define PM_SUSPEND_MEM       3
#define PM_SUSPEND_MAX       4

typedef int suspend_state_t;

struct platform_suspend_ops {
    int (*valid)(suspend_state_t state);
    int (*enter)(suspend_state_t state);
};

struct platform_s2idle_ops {
    int (*wake)(void);
    void (*check)(void);
};

enum {
    S2IDLE_STATE_NONE = 0,
    S2IDLE_STATE_ENTER,
    S2IDLE_STATE_WAKE,
};

extern const char *const pm_labels[];
extern const char *pm_states[PM_SUSPEND_MAX];
extern const char *mem_sleep_states[PM_SUSPEND_MAX];
extern suspend_state_t mem_sleep_current;
extern suspend_state_t mem_sleep_default;
extern suspend_state_t pm_suspend_target_state;
extern unsigned int pm_suspend_global_flags;
extern int pm_async_enabled;

void pm_restore_gfp_mask(void);
void pm_restrict_gfp_mask(void);
unsigned int lock_system_sleep(void);
void unlock_system_sleep(unsigned int flags);
int suspend_valid_only_mem(suspend_state_t state);
void suspend_set_ops(const struct platform_suspend_ops *ops);
void power_suspend_init(void);
int power_suspend_enter(suspend_state_t state);
bool pm_suspend_default_s2idle(void);
void s2idle_set_ops(const struct platform_s2idle_ops *ops);
void s2idle_wake(void);

int pm_notifier_call_chain(unsigned long val);
int pm_notifier_call_chain_robust(unsigned long val_up, unsigned long val_down);

void poweroff_trigger(void);
void poweroff_force(void);

void power_system_reset(void);
void power_system_shutdown(void);
int power_system_suspend(void);

#endif /* POWER_H */
