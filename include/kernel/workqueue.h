#ifndef GALIO_WORKQUEUE_H
#define GALIO_WORKQUEUE_H

#include "common.h"

typedef void (*workqueue_fn)(void *arg);

void workqueue_init(void);
int workqueue_schedule(workqueue_fn function, void *arg);
void workqueue_run(void);

#endif /* GALIO_WORKQUEUE_H */