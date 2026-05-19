/* memory.c - Memory + disk persistence glue */

#include "memory.h"
#include "vfs.h"
#include "vfs_core.h"
#include "ext2.h"
#include "kprintf.h"

void memory_init_disk_persistence(void) {
    /* Storage subsystems are initialized elsewhere (ATA/ext2/vfs).
       This function just centralizes “disk persistence readiness”. */
    if (vfs_core_is_disk_mode()) {
        kprintf("[MEM] Disk persistence initialized (disk mode)\n");
    } else {
        kprintf("[MEM] Disk persistence initialized (RAM mode)\n");
    }
}

void memory_disk_fsync(void) {
    if (!vfs_core_is_disk_mode()) return;

    /* Prefer the VFS sync entrypoint. */
    if (vfs_fsync()) return;

    /* Fallback: force EXT2 sync directly. */
    (void)ext2_fsync();
    kprintf("[MEM] Disk fsync forced\n");
}