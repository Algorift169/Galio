#ifndef ATA_H
#define ATA_H

#include "common.h"

#define ATA_PRIMARY_IO    0x1F0
#define ATA_PRIMARY_CTRL  0x3F6
#define ATA_SECONDARY_IO  0x170
#define ATA_SECONDARY_CTRL 0x376

#define ATA_DATA       0x00
#define ATA_ERROR      0x01
#define ATA_FEATURES   0x01
#define ATA_SECCOUNT   0x02
#define ATA_LBA_LOW    0x03
#define ATA_LBA_MID    0x04
#define ATA_LBA_HIGH   0x05
#define ATA_DRIVE      0x06
#define ATA_COMMAND    0x07
#define ATA_STATUS     0x07

#define ATA_CMD_READ_SECTORS  0x20
#define ATA_CMD_WRITE_SECTORS 0x30
#define ATA_CMD_IDENTIFY      0xEC
#define ATA_CMD_CACHE_FLUSH   0xE7

#define ATA_STATUS_BSY  0x80
#define ATA_STATUS_RDY  0x40
#define ATA_STATUS_DRQ  0x08
#define ATA_STATUS_ERR  0x01

void ata_init(void);
i32 ata_read_sectors(u32 lba, u32 count, void *buffer);
i32 ata_write_sectors(u32 lba, u32 count, const void *buffer);
i32 ata_flush_cache(void);
u32 ata_get_sectors(void);

#endif /* ATA_H */
