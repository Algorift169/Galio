#ifndef GALIO_ACPI_H
#define GALIO_ACPI_H

#include "common.h"

int acpi_init(void);
u8 acpi_is_available(void);
u8 acpi_has_sleep_state(u8 state);
u64 acpi_mcfg_base(void);
u64 acpi_hpet_base(void);
u16 acpi_pm1a_control_port(void);
u16 acpi_pm1b_control_port(void);
u16 acpi_pm1_control_length(void);
int acpi_enter_sleep(u8 state);

#endif
