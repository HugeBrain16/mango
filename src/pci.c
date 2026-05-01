#include "pci.h"
#include "io.h"

uint32_t pci_addr(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
	return PCI_ENABLE |
	(bus << 16) |
	(dev << 11) |
	(func << 8) |
	(offset & 0xFC);
}

uint32_t pci_read_config(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
	uint32_t address = pci_addr(bus, dev, func, offset);

	outl(PCI_CMD, address);
	return inl(PCI_DATA);
}

void pci_write_config(uint32_t value, uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
	uint32_t address = pci_addr(bus, dev, func, offset);
	outl(PCI_CMD, address);
	outl(PCI_DATA, value);
}

int pci_get_device(pci_device_t *device, uint8_t bus, uint8_t dev) {
	uint32_t id = pci_read_config(bus, dev, 0, PCI_REG_ID);
	if (PCI_VENDOR(id) == 0xFFFF)
		return 0;

	uint32_t code = pci_read_config(bus, dev, 0, PCI_REG_CODE);
	uint32_t header = pci_read_config(bus, dev, 0, 0xC); // only for getting header type

	if (device) {
		device->bus = bus;
		device->dev = dev;

		device->device_id = PCI_DEVICE(id);
		device->vendor_id = PCI_VENDOR(id);

		device->class_code = (code >> 24) & 0xFF;
		device->subclass   = (code >> 16) & 0xFF;
		device->interface  = (code >>  8) & 0xFF;
		device->revision   = (code) 	  & 0xFF;

		device->header_type = (header >> 16) & 0xFF;
	}
	return 1;
}

uint32_t pci_device_read(pci_device_t *device, uint8_t func, uint8_t offset) {
	return pci_read_config(device->bus, device->dev, func, offset);
}

void pci_device_write(pci_device_t *device, uint32_t value, uint8_t func, uint8_t offset) {
	pci_write_config(value, device->bus, device->dev, func, offset);
}
