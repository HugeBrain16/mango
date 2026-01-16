#include "ata.h"
#include "io.h"

static void ata_wait_ready() {
    while (inb(ATA_PORT_STATUS) & ATA_STATUS_BSY);
}

static void ata_wait_data() {
    while (!(inb(ATA_PORT_STATUS) & ATA_STATUS_DRQ));
}

void ata_init() {
    outb(ATA_PORT_DRIVE, ATA_DRV_BASE | ATA_DRV_LBA);

    // 400ns delay
    for (int i = 0; i < 4; i++)
        inb(ATA_PORT_STATUS);
}

static void ata_prepare(uint32_t lba, uint8_t command) {
    ata_wait_ready();

    uint8_t drive = ATA_DRV_BASE | ATA_DRV_LBA | ((command != ATA_IDENTIFY) ? ((lba >> 24) & ATA_DRV_HEAD_MASK) : 0);
    outb(ATA_PORT_DRIVE, drive);

    if (command != ATA_IDENTIFY) {
        outb(ATA_PORT_SECTOR, 1);
        outb(ATA_PORT_LBA_LO, (uint8_t) lba);
        outb(ATA_PORT_LBA_MID, (uint8_t) (lba >> 8));
        outb(ATA_PORT_LBA_HI, (uint8_t) (lba >> 16));
    } else {
        outb(ATA_PORT_SECTOR, 0);
        outb(ATA_PORT_LBA_LO, 0);
        outb(ATA_PORT_LBA_MID, 0);
        outb(ATA_PORT_LBA_HI, 0);
    }

    outb(ATA_PORT_COMMAND, command);
    ata_wait_data();
}

int ata_identify(uint16_t *buffer) {
    ata_prepare(0, ATA_IDENTIFY);

    uint8_t status = inb(ATA_PORT_STATUS);
    if (status == 0xFF || status & ATA_STATUS_ERR) return 0;

    for (int i = 0; i < 256; i++)
        buffer[i] = inw(ATA_PORT_DATA);

    return 1;
}

int ata_read_sector(uint32_t lba, uint16_t *buffer) {
    ata_prepare(lba, ATA_READ);

    uint8_t status = inb(ATA_PORT_STATUS);
    if (status & ATA_STATUS_ERR) return 0;

    for (int i = 0; i < 256; i++)
        buffer[i] = inw(ATA_PORT_DATA);

    return 1;
}

int ata_write_sector(uint32_t lba, uint16_t *buffer) {
    ata_prepare(lba, ATA_WRITE);

    uint8_t status = inb(ATA_PORT_STATUS);
    if (status & ATA_STATUS_ERR) return 0;

    for (int i = 0; i < 256; i++)
        outw(ATA_PORT_DATA, buffer[i]);

    outb(ATA_PORT_COMMAND, ATA_FLUSH);
    ata_wait_ready();

    return 1;
}
