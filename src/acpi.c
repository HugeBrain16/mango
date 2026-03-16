#include "acpi.h"
#include "string.h"
#include "io.h"
#include "pit.h"
#include "kernel.h"

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

sdt_t *acpi_find_table(const char *signature) {
	int entries = (acpi_rsdt->header.length - sizeof(acpi_rsdt->header)) / 4;
	for (int i = 0; i < entries; i++) {
		sdt_t *header = (sdt_t*) acpi_rsdt->tables[i];
		if (!memcmp(header->signature, signature, 4))
			return header;
	}

	return NULL;
}

int acpi_mode_enabled() {
	return (acpi_rsdp && inw(acpi_fadt->pm1a_control_block) & 1);
}

void acpi_shutdown() {
	int aml_length = acpi_dsdt->header.length - sizeof(acpi_dsdt->header);
    char *aml = acpi_dsdt->aml;

    for (int i = 0; i < aml_length - 5; i++) {
        if (aml[i] == 0x08 && !memcmp(&aml[i+1], "_S5_", 4)) {
            char *p = &aml[i + 5];

            if (*p != 0x12)
                continue;
            p += 3;

            if (*p == 0x0A) p++;

            uint8_t slp_typ_a = *p;
            p++;

            if (*p == 0x0A) p++;

            uint8_t slp_typ_b = *p;

            uint16_t SLP_TYPa = slp_typ_a << 10;
            uint16_t SLP_TYPb = slp_typ_b << 10;

            uint16_t SLP_EN = 1 << 13;

            outw(acpi_fadt->pm1a_control_block, SLP_TYPa | SLP_EN);
            if (acpi_fadt->pm1b_control_block)
                outw(acpi_fadt->pm1b_control_block, SLP_TYPb | SLP_EN);
        }
    }
}

void acpi_init() {
	char buffer[64];

	log("[ INFO ] ACPI Initializing...\n");

	acpi_rsdp = acpi_find_rsdp();
	if (acpi_rsdp)
		strfmt(buffer, "[ INFO ] ACPI RSDP: 0x%x\n", acpi_rsdp);
	else
		strfmt(buffer, "[ WARNING ] Could not find RSDP!\n");
	log(buffer);

	if (acpi_rsdp) {
		acpi_rsdt = (rsdt_t*)acpi_rsdp->rsdt_addr;
		strfmt(buffer, "[ INFO ] ACPI RSDT: 0x%x\n", acpi_rsdt);
		log(buffer);

		acpi_fadt = (fadt_t*)acpi_find_table("FACP");
		strfmt(buffer, "[ INFO ] ACPI FADT: 0x%x\n", acpi_fadt);
		log(buffer);

		acpi_dsdt = (dsdt_t*)acpi_fadt->dsdt;
		strfmt(buffer, "[ INFO ] ACPI DSDT: 0x%x\n", acpi_dsdt);
		log(buffer);

		if (acpi_fadt->smi_command_port != 0) {
	        log("[ INFO ] ACPI mode is not enabled, attempting to enable...\n");
	        outb(acpi_fadt->smi_command_port, acpi_fadt->acpi_enable);

	        uint32_t start = pit_ticks;
	        while (!acpi_mode_enabled()) {
	            if (pit_ticks - start > 3000) {
	                log("[ WARNING ] ACPI intialization timed out!\n");
	                break;
	            }
	        }
	    }
	}

    if (acpi_mode_enabled())
        log("[ INFO ] ACPI OK\n");
    else
        log("[ WARNING ] ACPI mode is not enabled!\n");
}