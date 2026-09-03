#include "libc_galio.h"

/* Syscall constants are shared by generated code and future user libc. */
int gc_libc_galio_present(void) { return GALIO_SYS_WRITE == 4; }
