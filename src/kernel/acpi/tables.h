#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "defs.h"

extern struct rsdp_table* rsdp;
extern struct rsdt_table* rsdt;
extern struct xsdt_table* xsdt;

extern struct fadt_table* fadt;
extern struct madt_table* madt;

extern acpi_revision_t acpi_revision;
extern uint32_t sdt_count;

static const char* preferred_power_management_profile_text[8] =
{
    "Unspecified",
    "Desktop",
    "Mobile",
    "Workstation",
    "Enterprise Server",
    "SOHO Server",
    "Appliance PC",
    "Performance Server"
};

void acpi_find_tables();
bool acpi_table_valid(void* table_address);
void* read_rsdt_ptr(uint32_t index);
