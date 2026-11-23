/*
 * Galio Kernel - lwIP sys_arch implementation (NO_SYS=1)
 *
 * In NO_SYS=1 mode lwIP only needs:
 *   u32_t sys_now(void)  -- millisecond timer
 *
 * All other sys_arch functions are excluded by the NO_SYS guard in lwIP itself.
 */

#include "lwip/opt.h"
#include "lwip/sys.h"
#include "drivers/pit.h"

/*
 * sys_now() - return current time in milliseconds.
 *
 * Galio's PIT runs at GALIO_HZ (1000 Hz), so each tick is one millisecond.
 */
u32_t sys_now(void)
{
    return (u32_t)pit_get_ticks();
}
