#ifndef ACPI_H
#define ACPI_H

#include <stdint.h>

typedef struct {
	char signature[8];
	uint8_t checksum;
	char oem_id[6];
	uint8_t revision;
	uint32_t rsdt_addr;
} __attribute__((packed)) rsdp_t;

typedef struct {
	char signature[4];
	uint32_t length;
	uint8_t revision;
	uint8_t checksum;
	char oem_id[6];
	char oem_table_id[8];
	uint32_t oem_revision;
	uint32_t creator_id;
	uint32_t creator_revision;
} __attribute__((packed)) sdt_t;

typedef struct {
	sdt_t header;
	uint32_t tables[];
} rsdt_t;

typedef struct {
    sdt_t header;
    uint32_t firmware_ctrl;
    uint32_t dsdt;

    uint8_t  reserved;

    uint8_t  preferred_power_management_profile;
    uint16_t sci_interrupt;
    uint32_t smi_command_port;
    uint8_t  acpi_enable;
    uint8_t  acpi_disable;
    uint8_t  s4bios_req;
    uint8_t  pstate_control;
    uint32_t pm1a_event_block;
    uint32_t pm1b_event_block;
    uint32_t pm1a_control_block;
    uint32_t pm1b_control_block;
    uint32_t pm2_control_block;
    uint32_t pm_timer_block;
    uint32_t gpe0_block;
    uint32_t gpe1_block;
    uint8_t  pm1_event_length;
    uint8_t  pm1_control_length;
    uint8_t  pm2_control_length;
    uint8_t  pm_timer_length;
    uint8_t  gpe0_length;
    uint8_t  gpe1_length;
    uint8_t  gpe1_base;
    uint8_t  cstate_control;
    uint16_t worst_c2_latency;
    uint16_t worst_c3_latency;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t  duty_offset;
    uint8_t  duty_width;
    uint8_t  day_alarm;
    uint8_t  month_alarm;
    uint8_t  century;
} __attribute__((packed)) fadt_t;

typedef struct {
    sdt_t header;
    char aml[];
} dsdt_t;

rsdp_t *acpi_rsdp;
rsdt_t *acpi_rsdt;
fadt_t *acpi_fadt;
dsdt_t *acpi_dsdt;

void acpi_init();
rsdp_t *acpi_find_rsdp();
sdt_t *acpi_find_table(const char *signature);
int acpi_mode_enabled();
void acpi_shutdown();

#endif