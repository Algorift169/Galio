#include "power/power.h"
#include "kprintf.h"

static int qos_value = 0;

int power_qos_get(void)
{
    return qos_value;
}

void power_qos_set(int value)
{
    qos_value = value;
    kprintf("[POWER] qos set to %d\n", qos_value);
}
