/* ata.c - ATA/IDE disk driver */

#include "ata.h"
#include "kprintf.h"
#include "cpu.h"

#define ATA_TIMEOUT 1000000

static u16 ata_io_base = ATA_PRIMARY_IO;
static u16 ata_ctrl_base = ATA_PRIMARY_CTRL;
static u32 ata_sector_count = 0;  /* Total sectors on disk */
static u32 ata_initialized = 0;

static u32 ata_irq_save(void) {
    u32 flags;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(flags) :: "memory");
    return flags;
}

static void ata_irq_restore(u32 flags) {
    if (flags & (1 << 9)) {
        __asm__ volatile("sti" ::: "memory");
    }
}

/* Wait for disk to be ready with timeout and status checking */
static i32 ata_wait(u8 mask, u8 value) {
    for (u32 i = 0; i < ATA_TIMEOUT; i++) {
        u8 status = inb(ata_io_base + ATA_STATUS);
        
        /* Check for error */
        if (status & ATA_STATUS_ERR) {
            return -1;
        }
        
        /* Check for desired status */
        if ((status & mask) == value) {
            return 0;
        }
        
        /* Small delay */
        for (volatile int j = 0; j < 10; j++);
    }
    return -1;
}

/* Wait for drive to be ready (no busy, ready set) */
static i32 ata_wait_ready(void) {
    for (u32 i = 0; i < ATA_TIMEOUT; i++) {
        u8 status = inb(ata_io_base + ATA_STATUS);
        
        if (status & ATA_STATUS_ERR) {
            kprintf("ATA: Error status: %x\n", status);
            return -1;
        }
        
        if (!(status & ATA_STATUS_BSY) && (status & ATA_STATUS_RDY)) {
            return 0;
        }
        
        for (volatile int j = 0; j < 10; j++);
    }
    return -1;
}

/* Select drive and set LBA */
static void ata_select_drive(u32 lba) {
    u8 drive = 0xE0 | ((lba >> 24) & 0x0F);  /* Master drive + LBA bits 24-27 */
    outb(ata_io_base + ATA_DRIVE, drive);
}

/* Set LBA registers */
static void ata_set_lba(u32 lba) {
    outb(ata_io_base + ATA_SECCOUNT, 1);  /* Sector count */
    outb(ata_io_base + ATA_LBA_LOW, lba & 0xFF);
    outb(ata_io_base + ATA_LBA_MID, (lba >> 8) & 0xFF);
    outb(ata_io_base + ATA_LBA_HIGH, (lba >> 16) & 0xFF);
    ata_select_drive(lba);
}

void ata_init(void) {
    kprintf("Initializing ATA driver...\n");

    /* Reset the controller */
    outb(ata_ctrl_base, 0x04);  /* Software reset */
    for (volatile int i = 0; i < 1000; i++);  /* Short delay */
    outb(ata_ctrl_base, 0x00);  /* Clear reset */
    
    /* Wait for drive to be ready after reset */
    for (volatile int i = 0; i < 100000; i++);  /* Longer delay for drive to spin up */
    
    if (ata_wait_ready() != 0) {
        kprintf("ATA: Drive not ready after reset\n");
        return;
    }

    /* Select master drive */
    outb(ata_io_base + ATA_DRIVE, 0xA0);  /* LBA mode, master */
    for (volatile int i = 0; i < 100; i++);
    
    /* Send IDENTIFY command */
    outb(ata_io_base + ATA_COMMAND, ATA_CMD_IDENTIFY);
    
    /* Wait for DRQ or error */
    if (ata_wait(ATA_STATUS_DRQ, ATA_STATUS_DRQ) != 0) {
        /* Check if device exists by reading status */
        u8 status = inb(ata_io_base + ATA_STATUS);
        if (status == 0xFF) {
            kprintf("ATA: No device present (status=0xFF)\n");
        } else if (status & ATA_STATUS_ERR) {
            kprintf("ATA: Identify command failed with error\n");
        } else {
            kprintf("ATA: Identify timeout, status=%x\n", status);
        }
        return;
    }

    /* Read identify data */
    u16 identify[256];
    for (int i = 0; i < 256; i++) {
        identify[i] = inw(ata_io_base + ATA_DATA);
    }

    /* Check if device is present (signature) */
    if (identify[0] == 0 || identify[0] == 0xFFFF) {
        kprintf("ATA: Invalid identify data, signature=0x%x\n", identify[0]);
        return;
    }

    ata_sector_count = identify[60] | (identify[61] << 16);
    ata_initialized = 1;
    
    if (ata_sector_count == 0) {
        kprintf("ATA: Warning - sector count is 0, using fallback\n");
        ata_sector_count = 1024 * 1024;  /* Fallback to 512 MB */
    }
}

i32 ata_read_sectors(u32 lba, u32 count, void *buffer) {
    if (!ata_initialized) {
        kprintf("ATA: Cannot read, driver not initialized\n");
        return -1;
    }
    
    u32 flags = ata_irq_save();
    
    u16 *buf = (u16 *)buffer;
    
    for (u32 i = 0; i < count; i++) {
        /* Wait for drive ready */
        if (ata_wait_ready() != 0) {
            kprintf("ATA: Read timeout - drive not ready at LBA %u\n", lba + i);
            ata_irq_restore(flags);
            return -1;
        }
        
        ata_set_lba(lba + i);
        outb(ata_io_base + ATA_COMMAND, ATA_CMD_READ_SECTORS);
        
        if (ata_wait(ATA_STATUS_DRQ, ATA_STATUS_DRQ) != 0) {
            kprintf("ATA: Read timeout at LBA %u\n", lba + i);
            ata_irq_restore(flags);
            return -1;
        }
        
        /* Read 256 words (512 bytes) */
        for (int j = 0; j < 256; j++) {
            buf[i * 256 + j] = inw(ata_io_base + ATA_DATA);
        }
    }
    
    ata_irq_restore(flags);
    return count;
}

i32 ata_write_sectors(u32 lba, u32 count, const void *buffer) {
    if (!ata_initialized) {
        kprintf("ATA: Cannot write, driver not initialized\n");
        return -1;
    }
    
    u32 flags = ata_irq_save();
    const u16 *buf = (const u16 *)buffer;
    
    for (u32 i = 0; i < count; i++) {
        /* Wait for drive ready */
        if (ata_wait_ready() != 0) {
            kprintf("ATA: Write timeout - drive not ready at LBA %u\n", lba + i);
            ata_irq_restore(flags);
            return -1;
        }
        
        ata_set_lba(lba + i);
        outb(ata_io_base + ATA_COMMAND, ATA_CMD_WRITE_SECTORS);
        
        if (ata_wait(ATA_STATUS_DRQ, ATA_STATUS_DRQ) != 0) {
            kprintf("ATA: Write timeout at LBA %u\n", lba + i);
            ata_irq_restore(flags);
            return -1;
        }
        
        for (int j = 0; j < 256; j++) {
            outw(ata_io_base + ATA_DATA, buf[i * 256 + j]);
        }
        
        /* Wait for write to complete */
        if (ata_wait(ATA_STATUS_BSY, 0) != 0) {
            kprintf("ATA: Write completion timeout at LBA %u\n", lba + i);
            ata_irq_restore(flags);
            return -1;
        }
    }
    
    ata_irq_restore(flags);
    return count;
}

i32 ata_flush_cache(void) {
    if (!ata_initialized) {
        kprintf("ATA: Cannot flush cache, driver not initialized\n");
        return -1;
    }
    
    u32 flags = ata_irq_save();
    
    /* Wait for drive to be ready */
    if (ata_wait_ready() != 0) {
        kprintf("ATA: Drive not ready for cache flush\n");
        ata_irq_restore(flags);
        return -1;
    }
    
    /* Select master drive */
    outb(ata_io_base + ATA_DRIVE, 0xA0);
    for (volatile int i = 0; i < 100; i++);
    
    /* Issue CACHE FLUSH command */
    outb(ata_io_base + ATA_COMMAND, ATA_CMD_CACHE_FLUSH);
    
    /* Wait for command to complete */
    if (ata_wait(ATA_STATUS_BSY, 0) != 0) {
        kprintf("ATA: Cache flush timeout\n");
        ata_irq_restore(flags);
        return -1;
    }
    
    ata_irq_restore(flags);
    return 0;
}

u32 ata_get_sectors(void) {
    return ata_initialized ? ata_sector_count : 0;
}