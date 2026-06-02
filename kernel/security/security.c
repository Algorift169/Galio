/* security.c - Security event handlers used to centralize fatal/soft security
 * responses. These wrappers make it easier to audit and change behavior later.
 */
#include "security/security.h"
#include "kprintf.h"
#include "common.h"

void security_warn(const char *msg) {
    kprintf("SECURITY WARNING: %s\n", msg);
}

void security_panic(const char *msg) {
    kprintf("SECURITY PANIC: %s\n", msg);
    panic(msg);
}
