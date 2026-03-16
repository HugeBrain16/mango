#include "acpi.h"
#include "string.h"

rsdp_t *acpi_find_rsdp() {
	uint32_t ebda = *(uint16_t*)0x40E << 4;
	for (uint32_t i = ebda; i < ebda + 1024; i += 16) {
		if (!memcmp((void*)i, "RSD PTR ", 8))
			return (rsdp_t*)i;
	}

	for (uint32_t i = 0xE0000; i < 0x100000; i += 16) {
		if (!memcmp((void*)i, "RSD PTR ", 8))
			return (rsdp_t*)i;
	}

	return NULL;
}

sdt_t *acpi_find_table(uint32_t rsdt_addr, const char *signature) {
	rsdt_t *rsdt = (void*)rsdt_addr;
	
	int entries = (rsdt->header.length - sizeof(rsdt->header)) / 4;
	for (int i = 0; i < entries; i++) {
		sdt_t *header = (sdt_t*) rsdt->tables[i];
		if (!memcmp(header->signature, signature, 4))
			return header;
	}

	return NULL;
}