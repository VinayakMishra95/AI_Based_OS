#include "ata.h"
#include "io.h"

#define ATA_DATA       0x1F0
#define ATA_ERR        0x1F1
#define ATA_SEC_CNT    0x1F2
#define ATA_LBA_LO     0x1F3
#define ATA_LBA_MID    0x1F4
#define ATA_LBA_HI     0x1F5
#define ATA_DRIVE_HEAD 0x1F6
#define ATA_STATUS     0x1F7
#define ATA_CMD        0x1F7

#define ATA_CMD_READ   0x20
#define ATA_CMD_WRITE  0x30
#define ATA_CMD_FLUSH  0xE7

static int ata_wait_bsy(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        if (!(inb(ATA_STATUS) & 0x80)) return 0;
    }
    return -1;
}

static int ata_wait_drq(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        uint8_t st = inb(ATA_STATUS);
        if (st & 0x01) return -1; // Error
        if (st & 0x08) return 0;  // DRQ ready
    }
    return -1;
}

int ata_read_sector(uint32_t lba, uint8_t* buf) {
    if (ata_wait_bsy() < 0) return -1;

    outb(ATA_DRIVE_HEAD, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    outb(ATA_SEC_CNT, 1);
    outb(ATA_LBA_LO,  (uint8_t)(lba & 0xFF));
    outb(ATA_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_LBA_HI,  (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_CMD, ATA_CMD_READ);

    if (ata_wait_bsy() < 0 || ata_wait_drq() < 0) return -1;

    uint16_t* ptr = (uint16_t*)buf;
    for (int i = 0; i < 256; i++) {
        ptr[i] = inw(ATA_DATA);
    }
    return 0;
}

int ata_write_sector(uint32_t lba, const uint8_t* buf) {
    if (ata_wait_bsy() < 0) return -1;

    outb(ATA_DRIVE_HEAD, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    outb(ATA_SEC_CNT, 1);
    outb(ATA_LBA_LO,  (uint8_t)(lba & 0xFF));
    outb(ATA_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_LBA_HI,  (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_CMD, ATA_CMD_WRITE);

    if (ata_wait_bsy() < 0 || ata_wait_drq() < 0) return -1;

    const uint16_t* ptr = (const uint16_t*)buf;
    for (int i = 0; i < 256; i++) {
        outw(ATA_DATA, ptr[i]);
    }

    outb(ATA_CMD, ATA_CMD_FLUSH);
    ata_wait_bsy();
    return 0;
}
