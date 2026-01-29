#include <stdint.h>
#include "../../libc/include/string.h"
#include "defs.h"
#include "../cpu/util.h"
#include <stdbool.h>

#include <bootboot.h>

extern BOOTBOOT bootboot;

struct rsdt_table* rsdt;
struct xsdt_table* xsdt;

physical_address_t rsdt_address;

struct fadt_table* fadt;
struct madt_table* madt;

uint8_t preferred_power_management_profile;

bool acpi_10;
uint32_t sdt_count;

#include "tables.h"

#include "../paging/paging.h"
#include "../cpu/memory.h"
#include "../ps2/ps2.h"

bool acpi_table_valid(physical_address_t table_address)
{
    uint8_t sum = 0;
    struct sdt_header* hdr = (struct sdt_header*)table_address;
    uint32_t length = hdr->length;
    for (uint32_t i = 0; i < length; i++)
        sum += ((uint8_t*)table_address)[i];
    return sum == 0;
}

void map_table_in_current_vas(uint64_t address, uint8_t privilege, uint8_t read_write)
{
    LOG(DEBUG, "Mapping table at address %#llx in memory", address);

    uint64_t* current_cr3 = (uint64_t*)(get_cr3() + PHYS_MAP_BASE);

    uint64_t aligned_address = address & 0xfffffffffffff000;
    struct sdt_header* hdr = (struct sdt_header*)address;
    uint32_t* length_ptr = &(hdr->length);
    
    uint64_t aligned_len_address = ((uint64_t)length_ptr) & 0xfffffffffffff000;

    remap_range(current_cr3, 
        aligned_len_address, aligned_len_address,
        1, privilege, read_write, CACHE_WB);
    invlpg(aligned_len_address);

    uint64_t mapping_offset = aligned_len_address != aligned_address ? 0 : 1;

    if ((((uint64_t)length_ptr) & 0xfff) > 0x1000 - 4)
    {
        mapping_offset = (mapping_offset == 1 ? 2 : 0);
        remap_range(current_cr3, 
            aligned_len_address + 0x1000, aligned_len_address + 0x1000,
            1, privilege, read_write, CACHE_WB);
        invlpg(aligned_len_address + 0x1000);
    }

    uint32_t length = *length_ptr;

    uint64_t last_address = (address + length - 1);

    uint64_t pages_to_map = (last_address - aligned_address + 0xfff) / 0x1000 - mapping_offset;
    remap_range(current_cr3, 
        aligned_address + 0x1000 * mapping_offset, aligned_address,
        pages_to_map, privilege, read_write, CACHE_WB);

    for (uint64_t i = mapping_offset; i < pages_to_map; i++)
        invlpg(aligned_address + 0x1000 * i);
}

void acpi_find_tables()
{
    rsdt_address = bootboot.arch.x86_64.acpi_ptr;
    rsdt = NULL;
    xsdt = NULL;
    fadt = NULL;

    acpi_10 = false;

    if (!rsdt_address)
    {
        LOG(ERROR, "Invalid ACPI setup");
        printf("Invalid ACPI setup\n");
        return;
    }

    {
        struct sdt_header* sdt = (struct sdt_header*)rsdt_address;
        map_table_in_current_vas(rsdt_address, PG_SUPERVISOR, PG_READ_ONLY);

        if (memcmp(sdt->signature, "RSDT", 4) == 0)
        {
            acpi_10 = true;
            rsdt = (struct rsdt_table*)rsdt_address;
            sdt_count = (rsdt->header.length - sizeof(struct sdt_header)) / 4;
        }
        else if (memcmp(sdt->signature, "XSDT", 4) == 0)
        {
            xsdt = (struct xsdt_table*)rsdt_address;
            sdt_count = (xsdt->header.length - sizeof(struct sdt_header)) / 8;
        }
        else
        {
            LOG(ERROR, "Invalid ACPI setup");
            printf("Invalid ACPI setup\n");
            return;
        }
    }

    LOG(INFO, "%u SDT tables detected", sdt_count);

    for (uint32_t i = 0; i < sdt_count; i++)
    {
        physical_address_t address = read_rsdt_ptr(i);
        LOG(INFO, "\tFound table at address %#llx", address);
        map_table_in_current_vas(address, PG_SUPERVISOR, PG_READ_ONLY);

        {
            if (acpi_table_valid(address))
            {
                struct sdt_header* sdt = (struct sdt_header*)address;
                uint32_t signature = *(uint64_t*)&sdt->signature;
                char signature_text[5] = { (char)signature, (char)(signature >> 8), (char)(signature >> 16), (char)(signature >> 24), 0 };
                LOG(INFO, "\t\tSignature: %s (0x%x)", signature_text, signature);
                printf("Signature: %s (0x%x)\n", signature_text, signature);
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
}

physical_address_t read_rsdt_ptr(uint32_t index)
{
    if (index >= sdt_count)
    {
        LOG(CRITICAL, "Kernel tried to read an invalid SDT (%u / %u)", index + 1, sdt_count);
        abort();
        return physical_null;
    }

    if (acpi_10)    // ACPI 1.0
    {
        physical_address_t sdt_ptr_start = sizeof(struct sdt_header) + rsdt_address;
        return *(uint32_t*)(sdt_ptr_start + 4 * index);
    }
    else
    {
        physical_address_t sdt_ptr_start = sizeof(struct sdt_header) + rsdt_address;
        return *(uint64_t*)(sdt_ptr_start + 8 * index);
    }

    return physical_null;
}

void fadt_extract_data()
{
    preferred_power_management_profile = 0;

    if (!fadt)
    {
        LOG(DEBUG, "No FADT found");
        ps2_controller_connected = true;
        return;
    }

    LOG(DEBUG, "Extracting data from the FADT");

    ps2_controller_connected = acpi_10 ? true : (fadt->boot_architecture_flags & 0b10) == 0b10;

    uint8_t _preferred_power_management_profile = fadt->preferred_power_management_profile;

    if (_preferred_power_management_profile > 7)
        preferred_power_management_profile = 0;
    else 
        preferred_power_management_profile = _preferred_power_management_profile;

    LOG(INFO, "Preferred power management profile : %s (%u)", _preferred_power_management_profile > 7 ? "Unknown" : preferred_power_management_profile_text[preferred_power_management_profile], _preferred_power_management_profile);
    // if (preferred_power_management_profile != 0)
    printf("Preferred power management profile : %s (%u)\n", _preferred_power_management_profile > 7 ? "Unknown" : preferred_power_management_profile_text[preferred_power_management_profile], _preferred_power_management_profile);
}
