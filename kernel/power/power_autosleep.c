#include "power/power.h"
#include "kprintf.h"

static int autosleep_enabled = 1;

void power_autosleep_set(int enabled)
{
    autosleep_enabled = enabled;
    kprintf("[POWER] autosleep %s\n", autosleep_enabled ? "enabled" : "disabled");
}

int power_autosleep_get(void)
{
    return autosleep_enabled;
}
