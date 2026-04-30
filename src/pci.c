#include "pci.h"
#include "io.h"

uint32_t pci_addr(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
	return PCI_ENABLE |
	(bus << 16) |
	(dev << 11) |
	(func << 8) |
	(offset & 0xFC);
}

uint32_t pci_get_config(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
	uint32_t address = pci_addr(bus, dev, func, offset);
	outl(PCI_CMD, address);

	return inl(PCI_DATA);
}

int pci_get_device(pci_device_t *device, uint8_t bus, uint8_t dev) {
	uint32_t id = pci_get_config(bus, dev, 0, PCI_REG_ID);
	if (PCI_VENDOR(id) == 0xFFFF)
		return 0;

	uint32_t code = pci_get_config(bus, dev, 0, PCI_REG_CODE);
	uint32_t header = pci_get_config(bus, dev, 0, 0xC); // only for getting header type

	if (device) {
		device->device_id = PCI_DEVICE(id);
		device->vendor_id = PCI_VENDOR(id);

		device->header_type = header >> 24;
		device->class_code = code & 0xFFFF;
		device->subclass = code >> 24;
		device->interface = code >> 16;
		device->revision = code >> 8;
	}
	return 1;
}