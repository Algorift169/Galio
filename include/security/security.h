/* security.h - Kernel security helpers */
#ifndef SECURITY_H
#define SECURITY_H

#include "common.h"

void security_warn(const char *msg);
void security_panic(const char *msg);

#endif /* SECURITY_H */
