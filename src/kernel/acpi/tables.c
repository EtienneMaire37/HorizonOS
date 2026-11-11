#include <stdint.h>
#include <string.h>
#include "defs.h"
#include <stdbool.h>
#include <limine.h>
#include "../util/error.h"
#include "../debug/out.h"

struct rsdp_table* rsdp;
struct rsdt_table* rsdt;
struct xsdt_table* xsdt;

struct fadt_table* fadt;
struct madt_table* madt;

acpi_revision_t acpi_revision = 0xff;
uint32_t sdt_count;

#include "../boot/limine.h"
#include "tables.h"
#include "../paging/paging.h"

bool acpi_table_valid(void* table_address)
{
    ASSERT(table_address);
    uint8_t sum = 0;
    struct sdt_header* hdr = (struct sdt_header*)table_address;
    uint32_t length = hdr->length;
    for (uint32_t i = 0; i < length; i++)
        sum += ((uint8_t*)table_address)[i];
    return sum == 0;
}

void acpi_find_tables()
{
    rsdp = rsdp_request.response->address;

    rsdt = NULL;
    xsdt = NULL;
    fadt = NULL;
    madt = NULL;

    if (!rsdp)
    {
        LOG(ERROR, "Invalid ACPI setup");
        printf("Invalid ACPI setup\n");
        return;
    }

    acpi_revision = rsdp->revision;

    switch (acpi_revision)
    {
    case ACPI_1_0:
        LOG(INFO, "ACPI version: 1.0");
        printf("ACPI version: 1.0\n");
        rsdt = (struct rsdt_table*)(rsdp->rsdt_address + PHYS_MAP_BASE);
        sdt_count = (rsdt->header.length - sizeof(struct sdt_header)) / 4;
        break;
    case ACPI_2_0_PLUS:
        LOG(INFO, "ACPI version: 2.0+");
        printf("ACPI version: 2.0+\n");
        xsdt = (struct xsdt_table*)(rsdp->xsdt_address + PHYS_MAP_BASE);
        sdt_count = (xsdt->header.length - sizeof(struct sdt_header)) / 8;
        break;
    default:
        LOG(ERROR, "Unknow ACPI revision");
        printf("Unknown ACPI revision\n");
        return;
    }

    LOG(INFO, "ACPI: %u SDT tables detected", sdt_count);

    for (uint32_t i = 0; i < sdt_count; i++)
    {
        void* address = read_rsdt_ptr(i);
        LOG(INFO, "\tFound table at address %p", address);

        if (acpi_table_valid(address))
        {
            struct sdt_header* sdt = (struct sdt_header*)address;
            uint32_t signature = sdt->signature;
            char signature_text[5] = { (char)signature, (char)(signature >> 8), (char)(signature >> 16), (char)(signature >> 24), 0 };
            LOG(INFO, "\t\tSignature: %s (%#x)", signature_text, signature);
            printf("Signature: %s (%#x)\n", signature_text, signature);
            switch (signature)
            {
            case 0x50434146:    // FACP : FADT
                LOG(INFO, "\t\tValid FADT");
                fadt = (struct fadt_table*)address;
                break;
            case 0x43495041:    // APIC : MADT
                LOG(INFO, "\t\tValid MADT");
                madt = (struct madt_table*)address;
                break;
            // case 0x54445344:    // DSDT : DSDT
            //     LOG(INFO, "\t\tValid DSDT");
            //     // dsdt_address = address;
            //     break;
            // case 0x54445353:    // SSDT : SSDT
            //     LOG(INFO, "\t\tValid SSDT");
            //     // ssdt_address = address;
            //     break;
            default:
                LOG(INFO, "\t\tUnkwown table");
            }
        }
    }
}

void* read_rsdt_ptr(uint32_t index)
{
    if (index >= sdt_count)
    {
        LOG(CRITICAL, "Kernel tried to read an invalid SDT (%u / %u)", index + 1, sdt_count);
        FATAL("Kernel tried to read an invalid SDT");
        return NULL;
    }

    switch (acpi_revision)
    {
    case ACPI_1_0:
        return (void*)(rsdt->ptrs_to_sdt[index] + PHYS_MAP_BASE);
    case ACPI_2_0_PLUS:
        return (void*)(xsdt->ptrs_to_sdt[index] + PHYS_MAP_BASE);
    default:
        return NULL;
    }
}
