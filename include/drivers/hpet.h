#ifndef GALIO_HPET_H
#define GALIO_HPET_H

#include "common.h"

int hpet_init(u64 physical_base);
int hpet_start_periodic(u32 frequency);
void hpet_stop_periodic(void);
u8 hpet_available(void);

#endif
