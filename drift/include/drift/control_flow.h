/* SPDX-License-Identifier: AGPL-3.0-only */
/* Loop control results are shared by statement execution and loop runtimes. */

#ifndef DRIFT_CONTROL_FLOW_H
#define DRIFT_CONTROL_FLOW_H

enum {
    DRIFT_EXECUTION_OK = 0,
    DRIFT_EXECUTION_ERROR = 1,
    DRIFT_EXECUTION_BREAK = 2,
    DRIFT_EXECUTION_CONTINUE = 3
};

#endif