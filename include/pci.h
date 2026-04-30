#ifndef PCI_H
#define PCI_H

#include <stdint.h>

#define PCI_MAX_BUS 256
#define PCI_MAX_DEV 32

#define PCI_ENABLE (uint32_t)0x80000000
#define PCI_CMD 0xCF8
#define PCI_DATA 0xCFC

/* used for named offset */
#define PCI_REG_ID 0x0
#define PCI_REG_IO 0x4
#define PCI_REG_CODE 0x8

#define PCI_VENDOR(config) ((config) & 0xFFFF)
#define PCI_DEVICE(config) ((config) >> 16)

typedef struct {
	uint8_t bus;
	uint8_t dev;

	uint16_t device_id;
	uint16_t vendor_id;
	uint8_t revision;

	uint8_t header_type;
	uint8_t class_code;
	uint8_t subclass;
	uint8_t interface;
} pci_device_t;

extern uint32_t pci_addr(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);
extern uint32_t pci_get_config(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);
extern int pci_get_device(pci_device_t *device, uint8_t bus, uint8_t dev);

#endif