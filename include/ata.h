#ifndef ATA_H
#define ATA_H

#include <stdint.h>

#define ATA_PRIMARY 	0x1F0
#define ATA_SECONDARY 	0x170

#define ATA_MASTER 	0
#define ATA_SLAVE 	1

#define ATA_PORT_DATA 		0
#define ATA_PORT_ERROR 		1
#define ATA_PORT_SECTOR 	2
#define ATA_PORT_LBA_LO 	3
#define ATA_PORT_LBA_MID 	4
#define ATA_PORT_LBA_HI 	5
#define ATA_PORT_DRIVE 		6
#define ATA_PORT_STATUS 	7
#define ATA_PORT_COMMAND 	7

#define ATA_CTRL_STATUS 0x3F6
#define ATA_CTRL_DEVICE 0x3F6
#define ATA_CTRL_DRIVE 	0x3F7

#define ATA_ERROR_AMNF 	(1 << 0) // address not found
#define ATA_ERROR_TKZNF (1 << 1) // track zero not found
#define ATA_ERROR_ABRT 	(1 << 2) // aborted command
#define ATA_ERROR_MCR 	(1 << 3) // media change request
#define ATA_ERROR_IDNF 	(1 << 4) // id not found
#define ATA_ERROR_MC 	(1 << 5) // media changed
#define ATA_ERROR_UNC 	(1 << 6) // uncorrectable data
#define ATA_ERROR_BBK 	(1 << 7) // bad block

#define ATA_DRV_HEAD_MASK 	0x0F
#define ATA_DRV_BASE 		0xA0
#define ATA_DRV_SELECT 		(1 << 4)
#define ATA_DRV_LBA 		(1 << 6)

#define ATA_STATUS_ERR 	(1 << 0) // error
#define ATA_STATUS_IDX 	(1 << 1) // index
#define ATA_STATUS_CORR (1 << 2) // corrected data
#define ATA_STATUS_DRQ 	(1 << 3) // pio ready
#define ATA_STATUS_SRV 	(1 << 4) // service
#define ATA_STATUS_DF 	(1 << 5) // drive fault error
#define ATA_STATUS_RDY 	(1 << 6) // ready
#define ATA_STATUS_BSY 	(1 << 7) // busy

#define ATA_READ 		0x20
#define ATA_WRITE 		0x30
#define ATA_FLUSH 		0xE7
#define ATA_IDENTIFY	0xEC

uint16_t ata_port(uint16_t base, uint8_t port);
uint8_t ata_status(uint16_t base);
void ata_command(uint16_t base, uint8_t command);
uint16_t ata_read(uint16_t base);
void ata_write(uint16_t base, uint16_t data);
void ata_clear_lba(uint16_t base);
void ata_set_lba(uint16_t base, uint32_t lba);
void ata_wait_io(uint16_t base);
void ata_wait_ready(uint16_t base);
void ata_wait_data(uint16_t base);
void ata_select(uint16_t base, uint8_t drive);
void ata_prepare(uint16_t base, uint32_t lba, uint8_t command);
int ata_identify(uint16_t base, void *buffer);
int ata_read_sector(uint16_t base, uint32_t lba, void *buffer);
int ata_write_sector(uint16_t base, uint32_t lba, void *buffer);

#endif
