#include "ata.h"
#include "io.h"

uint16_t ata_port(uint16_t base, uint8_t port) {
    return (uint16_t) base + port;
}

uint8_t ata_status(uint16_t base) {
    return inb(ata_port(base, ATA_PORT_STATUS));
}

void ata_command(uint16_t base, uint8_t command) {
    outb(ata_port(base, ATA_PORT_COMMAND), command);
}

uint16_t ata_read(uint16_t base) {
    return inw(ata_port(base, ATA_PORT_DATA));
}

void ata_write(uint16_t base, uint16_t data) {
    outw(ata_port(base, ATA_PORT_DATA), data);
}

void ata_clear_lba(uint16_t base) {
    outb(ata_port(base, ATA_PORT_SECTOR), 0);
    outb(ata_port(base, ATA_PORT_LBA_LO), 0);
    outb(ata_port(base, ATA_PORT_LBA_MID), 0);
    outb(ata_port(base, ATA_PORT_LBA_HI), 0);
}

void ata_set_lba(uint16_t base, uint32_t lba) {
    outb(ata_port(base, ATA_PORT_SECTOR), 1);
    outb(ata_port(base, ATA_PORT_LBA_LO), (uint8_t) lba);
    outb(ata_port(base, ATA_PORT_LBA_MID), (uint8_t) (lba >> 8));
    outb(ata_port(base, ATA_PORT_LBA_HI), (uint8_t) (lba >> 16));
}

void ata_wait_io(uint16_t base) {
    for (int i = 0; i < 4; i++)
        inb(ata_port(base, ATA_PORT_STATUS));
}

void ata_wait_ready(uint16_t base) {
    while (inb(ata_port(base, ATA_PORT_STATUS)) & ATA_STATUS_BSY);
}

void ata_wait_data(uint16_t base) {
    while (!(inb(ata_port(base, ATA_PORT_STATUS)) & ATA_STATUS_DRQ));
}

void ata_select(uint16_t base, uint8_t drive) {
    outb(ata_port(base, ATA_PORT_DRIVE), (ATA_DRV_BASE + drive) | ATA_DRV_LBA);   
}

void ata_prepare(uint16_t base, uint32_t lba, uint8_t command) {
    ata_wait_ready(base);

    if (command != ATA_IDENTIFY)
        ata_set_lba(base, lba);
    else
        ata_clear_lba(base);

    ata_command(base, command);
    ata_wait_data(base);
}

int ata_identify(uint16_t base, void *buffer) {
    ata_prepare(base, 0, ATA_IDENTIFY);

    uint8_t status = ata_status(base);
    if (status == 0xFF || status & ATA_STATUS_ERR) return 0;

    uint16_t *word = (uint16_t*) buffer;

    for (int i = 0; i < 256; i++)
        word[i] = ata_read(base);

    return 1;
}

int ata_read_sector(uint16_t base, uint32_t lba, void *buffer) {
    ata_prepare(base, lba, ATA_READ);

    uint8_t status = ata_status(base);
    if (status & ATA_STATUS_ERR) return 0;

    uint16_t *word = (uint16_t*) buffer;

    for (int i = 0; i < 256; i++)
        word[i] = ata_read(base);

    return 1;
}

int ata_write_sector(uint16_t base, uint32_t lba, void *buffer) {
    ata_prepare(base, lba, ATA_WRITE);

    uint8_t status = ata_status(base);
    if (status & ATA_STATUS_ERR) return 0;

    uint16_t *word = (uint16_t*) buffer;

    for (int i = 0; i < 256; i++)
        ata_write(base, word[i]);

    ata_command(base, ATA_FLUSH);
    ata_wait_ready(base);

    return 1;
}
