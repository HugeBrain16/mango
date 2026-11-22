#include "ata.h"
#include "io.h"
#include "pit.h"
#include "string.h"

#define CHECK_READY(base) if (!ata_wait_ready(base)) return 0

static uint16_t last_id = 0;
static uint32_t last_sectors_count = 0;

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

int ata_wait_ready(uint16_t base) {
    uint32_t start = pit_ticks;
    while (1) {
        uint8_t status = ata_status(base);

        if (status == 0x00 || status == 0xFF)
            return 0;
        if (!(status & ATA_STATUS_BSY)) {
            if (status & ATA_STATUS_ERR || status & ATA_STATUS_DF)
                return 0;
            if (status & ATA_STATUS_RDY)
                return 1;
        }

        if (pit_ticks - start > 1000)
            return 0;
    }

    return 0;
}

int ata_wait_data(uint16_t base) {
    uint32_t start = pit_ticks;
    while (!(ata_status(base) & ATA_STATUS_DRQ)) {
        uint8_t status = ata_status(base);
        if (status & ATA_STATUS_ERR || status & ATA_STATUS_DF || pit_ticks - start > 5000)
            return 0;
    }

    return 1;
}

void ata_select(uint16_t base, uint8_t drive) {
    outb(ata_port(base, ATA_PORT_DRIVE), ATA_DRV_BASE | (drive << 4) | ATA_DRV_LBA);
    ata_wait_io(base);
}

int ata_prepare(uint16_t base, uint32_t lba, uint8_t command) {
    if (!ata_wait_ready(base))
        return 0;

    if (command != ATA_IDENTIFY)
        ata_set_lba(base, lba);
    else
        ata_clear_lba(base);

    ata_command(base, command);
    ata_wait_io(base);

    return ata_wait_data(base);
}

int ata_identify(uint16_t base, void *buffer) {
    if (!ata_prepare(base, 0, ATA_IDENTIFY))
        return 0;

    uint8_t status = ata_status(base);
    if (status == 0xFF || status & ATA_STATUS_ERR) return 0;

    uint16_t *word = (uint16_t*) buffer;

    for (int i = 0; i < 256; i++)
        word[i] = ata_read(base);

    return 1;
}

uint32_t ata_count_sectors(uint16_t *w) {
    return w[49] & (1 << 9) ? (uint32_t)w[60] | ((uint32_t)w[61] << 16) : (uint32_t)w[1] * w[3] * w[6];
}

static int ata_lba_check(uint16_t base, uint32_t lba) {
    uint8_t ata_id[512];
    uint32_t sectors = 0;

    if (last_id != base) {
        if (!ata_identify(base, ata_id))
            return 0;

        sectors = ata_count_sectors((uint16_t*)ata_id);
        if (sectors == 0)
            return 0;

        last_id = base;
        last_sectors_count = sectors;
    } else {
        sectors = last_sectors_count;
    }

    return lba < sectors;
}

int ata_read_sector(uint16_t base, uint32_t lba, void *buffer) {
    if (!ata_lba_check(base, lba))
        return 0;

    if (!ata_prepare(base, lba, ATA_READ))
        return 0;

    uint8_t status = ata_status(base);
    if (status & ATA_STATUS_ERR) return 0;

    uint16_t *word = (uint16_t*) buffer;

    for (int i = 0; i < 256; i++)
        word[i] = ata_read(base);

    CHECK_READY(base);

    return 1;
}

int ata_write_sector(uint16_t base, uint32_t lba, void *buffer) {
    if (!ata_lba_check(base, lba))
        return 0;

    if (!ata_prepare(base, lba, ATA_WRITE))
        return 0;

    uint8_t status = ata_status(base);
    if (status & ATA_STATUS_ERR) return 0;

    uint16_t *word = (uint16_t*) buffer;

    for (int i = 0; i < 256; i++)
        ata_write(base, word[i]);

    CHECK_READY(base);
    ata_command(base, ATA_FLUSH);
    CHECK_READY(base);

    ata_wait_io(base);
    ata_wait_ready(base);

    return 1;
}

int ata_get_string(uint16_t *w, int start, int end, char *dest, size_t size) {
    int j = 0;

    for (int i = start; i < end; i++) {
        if (j + 2 < (int)size) {
            dest[j++] = (char)((w[i] >> 8) & 0xFF);
            dest[j++] = (char)(w[i] & 0xFF);
        }
    }

    dest[j] = '\0';
    strrtrim(dest);

    if (j < end)
        return 0;
    return 1;
}
