#include "power/power.h"
#include "kprintf.h"

int power_console_suspend(void)
{
    kprintf("[POWER] console suspend path\n");
    return 0;
}

int power_console_resume(void)
{
    kprintf("[POWER] console resume path\n");
    return 0;
}
