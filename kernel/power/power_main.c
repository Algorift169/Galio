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

#include "power/power.h"
#include "acpi/acpi.h"
#include "kprintf.h"
#include "string.h"
#include "arch/x86/cpu.h"
#include "drivers/pit.h"
#include "drivers/hpet.h"
#include "net/netdev.h"
#include "sound/sound.h"
#include <stdbool.h>

void poweroff_trigger(void)
{
    kprintf("[POWER] poweroff requested\n");
}

void poweroff_force(void)
{
    kprintf("[POWER] forced shutdown path\n");
}

void power_system_reset(void)
{
    kprintf("[POWER] reboot requested - initiating reset sequence\n");
    for (int i = 0; i < 100000; i++) {
        asm volatile("nop");
    }
    outb(0xCF9, 0x02);
    outb(0xCF9, 0x06);
    while (1) {
        asm volatile("hlt");
    }
}

void power_system_shutdown(void)
{
    kprintf("[POWER] shutdown requested - powering off\n");

    if (acpi_enter_sleep(5u) == 0) {
        asm volatile("cli");
        while (1) asm volatile("hlt");
    }

    /* QEMU exposes the ACPI power-management control register at 0x604.
     * Keep the legacy ports as fallbacks for other emulators/firmware. */
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);

    asm volatile("cli");
    while (1) {
        asm volatile("hlt");
    }
}

int power_system_suspend(void)
{
    net_device_t *net_device;
    sound_device_t *sound_device;
    int result;

    if (!acpi_has_sleep_state(3u)) {
        kprintf("[POWER] suspend not supported: ACPI S3 is unavailable\n");
        return -38;
    }
    kprintf("[POWER] entering ACPI S3 suspend\n");
    for (net_device = netdev_first(); net_device; net_device = netdev_next(net_device)) {
        if ((net_device->flags & NETIF_UP) && net_device->stop)
            net_device->stop(net_device);
    }
    for (sound_device = sound_device_first(); sound_device; sound_device = sound_device->next) {
        if (sound_device->active_stream)
            sound_stream_stop(sound_device->active_stream);
        if (sound_device->ops && sound_device->ops->stop)
            sound_device->ops->stop(sound_device);
    }
    pit_disable();
    hpet_stop_periodic();
    result = acpi_enter_sleep(3u);
    if (result == 0) {
        for (net_device = netdev_first(); net_device; net_device = netdev_next(net_device)) {
            if (net_device->open) net_device->open(net_device);
        }
        for (sound_device = sound_device_first(); sound_device; sound_device = sound_device->next) {
            if (sound_device->ops && sound_device->ops->init)
                sound_device->ops->init(sound_device);
        }
        pit_enable();
        (void)hpet_start_periodic(1000u);
    }
    return result;
}

const char *const pm_labels[] = {
    [PM_SUSPEND_TO_IDLE] = "freeze",
    [PM_SUSPEND_STANDBY] = "standby",
    [PM_SUSPEND_MEM] = "mem",
};

suspend_state_t mem_sleep_current = PM_SUSPEND_TO_IDLE;
suspend_state_t mem_sleep_default = PM_SUSPEND_MAX;
suspend_state_t pm_suspend_target_state = PM_SUSPEND_ON;
unsigned int pm_suspend_global_flags = 0;
int pm_async_enabled = 1;
