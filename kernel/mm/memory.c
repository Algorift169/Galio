/*
 * Galio Kernel
 *
 * Copyright (C) 2026 S.M Israfil
 *
 * This file is part of Galio.
 *
 * Galio is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * Galio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Galio. If not, see <https://www.gnu.org/licenses/>.
 */

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