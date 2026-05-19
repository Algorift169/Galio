#ifndef MEMORY_H
#define MEMORY_H

#include "common.h"

/* Initializes memory + disk-backed persistence plumbing */
void memory_init_disk_persistence(void);

/* Forces any disk-backed persistence to be flushed */
void memory_disk_fsync(void);

#endif /* MEMORY_H */