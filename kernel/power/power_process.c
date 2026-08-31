#include "power/power.h"
#include "kprintf.h"

static int freezer_enabled = 1;
static int freezer_active = 0;

void power_freezer_enable(void)
{
    freezer_enabled = 1;
}

void power_freezer_disable(void)
{
    freezer_enabled = 0;
}

int power_freezer_is_active(void)
{
    return freezer_active;
}

void power_freezer_activate(void)
{
    freezer_active = freezer_enabled;
    kprintf("[POWER] freezer activated\n");
}

void power_freezer_deactivate(void)
{
    freezer_active = 0;
    kprintf("[POWER] freezer deactivated\n");
}
