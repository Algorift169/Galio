#ifndef GALIO_WORKQUEUE_H
#define GALIO_WORKQUEUE_H

#include "common.h"

typedef void (*workqueue_fn)(void *arg);
typedef workqueue_fn work_fn_t;

void workqueue_init(void);
int workqueue_schedule(workqueue_fn function, void *arg);
int workqueue_cancel(work_fn_t function, void *arg);
void workqueue_flush_fn(work_fn_t function, void *arg);
void workqueue_run(void);

#endif /* GALIO_WORKQUEUE_H */