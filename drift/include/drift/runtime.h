/* SPDX-License-Identifier: AGPL-3.0-only */
#ifndef DRIFT_RUNTIME_H
#define DRIFT_RUNTIME_H

#include "drift/environment.h"

int drift_execute_source(const char *source, Environment *environment);

#endif
