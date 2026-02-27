#include "defs.h"

// * page 0xFEE00xxx
volatile local_apic_registers_t* lapic = NULL;

uint32_t ps2_1_gsi = 1, ps2_12_gsi = 12;

#include "apic.h"
#include "../acpi/tables.h"
#include "../paging/paging.h"
#include "../cpu/memory.h"
#include "../ps2/ps2.h"
#include "../cpu/msr.h"

void apic_init()
{
    uint64_t apic_base_msr = rdmsr(IA32_APIC_BASE_MSR);

    lapic = (volatile local_apic_registers_t*)((apic_base_msr & ~0xfffULL) + PHYS_MAP_BASE);

    // * Disable x2APIC and enable
    apic_base_msr &= ~(1ULL << 10);
    wrmsr(IA32_APIC_BASE_MSR, apic_base_msr);

    apic_base_msr |= (1ULL << 11);
    wrmsr(IA32_APIC_BASE_MSR, apic_base_msr);
}

uint8_t apic_get_cpu_id()
{
    return (lapic->id_register >> 24) & 0xff;
}

void lapic_send_eoi()
{
    lapic->end_of_interrupt_register = 0;
}

void lapic_set_spurious_interrupt_number(uint8_t int_num)
{
    uint32_t val = lapic->spurious_interrupt_vector_register;
    val &= 0xffffff00;
    val |= int_num;
    lapic->spurious_interrupt_vector_register = val;
}

void lapic_enable()
{
    lapic->spurious_interrupt_vector_register |= 0x100;
}

void lapic_disable()
{
    lapic->spurious_interrupt_vector_register &= ~0x100;
}

void lapic_set_tpr(uint8_t p)
{
    uint32_t val = lapic->task_priority_register;
    val &= 0xffffff00;
    val |= p;
    lapic->task_priority_register = val;
}

uint32_t ioapic_read_register(volatile io_apic_registers_t* ioapic, uint8_t reg)
{
    ioapic->IOREGSEL = reg;
    memory_barrier();
    return ioapic->IOWIN;
}

void ioapic_write_register(volatile io_apic_registers_t* ioapic, uint8_t reg, uint32_t val)
{
    ioapic->IOREGSEL = reg;
    memory_barrier();
    ioapic->IOWIN = val;
    memory_barrier();
    (void)ioapic->IOWIN;
}

uint8_t ioapic_get_max_redirection_entry(volatile io_apic_registers_t* ioapic)
{
    return (ioapic_read_register(ioapic, IOAPICVER) >> 16) & 0xff;
}

uint64_t ioapic_read_redirection_entry(volatile io_apic_registers_t* ioapic, uint32_t entry)
{
    if (entry > ioapic_get_max_redirection_entry(ioapic)) 
    {
        LOG(ERROR, "ioapic_read_redirection_entry: Invalid redirection entry !!!");
        return 0;
    }

    uint32_t low = ioapic_read_register(ioapic, IOAPICREDTBL(entry));
    uint32_t high = ioapic_read_register(ioapic, IOAPICREDTBL(entry) + 1);

    return ((uint64_t)high << 32) | low;
}

void ioapic_write_redirection_entry(volatile io_apic_registers_t* ioapic, uint32_t entry, uint64_t value)
{
    if (entry > ioapic_get_max_redirection_entry(ioapic)) 
    {
        LOG(ERROR, "ioapic_write_redirection_entry: Invalid redirection entry !!!");
        return;
    }

    ioapic_write_register(ioapic, IOAPICREDTBL(entry), value & 0xffffffff);
    ioapic_write_register(ioapic, IOAPICREDTBL(entry) + 1, value >> 32);
}

void map_ioapic_in_current_vas(uint64_t address, uint8_t privilege, uint8_t read_write)
{
    LOG(DEBUG, "Mapping I/O APIC at address %#" PRIx64 " in memory", address);

    remap_range((uint64_t*)(get_cr3() + PHYS_MAP_BASE), 
        address, address,
        1, privilege, read_write, CACHE_WB);
    invlpg(address);
}

struct madt_entry_header* find_entry_in_madt(bool (*test_func)(struct madt_entry_header*))
{
    if (!test_func) return NULL;

    struct madt_entry_header* header = (struct madt_entry_header*)((uint64_t)madt + 0x2C);
    uint32_t offset = 0x2c;
    while (offset < madt->header.length)
    {
        if (header->record_length + offset > madt->header.length) break;

        if (test_func(header))
            return header;

        if (offset > 0xffffffff - header->record_length) break;
        offset += header->record_length;
        header = (struct madt_entry_header*)((uint64_t)header + header->record_length);
    }

    return NULL;
}

bool is_madt_entry_irq_source_1(struct madt_entry_header* header)
{
    if (header->entry_type == 2)    // * I/O APIC Interrupt Source Override
    {
        struct madt_ioapic_interrupt_source_override_entry* entry = (struct madt_ioapic_interrupt_source_override_entry*)header;
        if (entry->irq_source == 1)
            return true;
    }
    return false;
}

bool is_madt_entry_irq_source_12(struct madt_entry_header* header)
{
    if (header->entry_type == 2)    // * I/O APIC Interrupt Source Override
    {
        struct madt_ioapic_interrupt_source_override_entry* entry = (struct madt_ioapic_interrupt_source_override_entry*)header;
        if (entry->irq_source == 12)
            return true;
    }
    return false;
}

bool is_madt_entry_irq_1_capable(struct madt_entry_header* header)
{
    if (header->entry_type == 1)    // * I/O APIC
    {
        struct madt_ioapic_entry* entry = (struct madt_ioapic_entry*)header;
        if (entry->gsi_base > ps2_1_gsi)
            return false;
        volatile io_apic_registers_t* ioapic = (volatile io_apic_registers_t*)((uint64_t)entry->ioapic_address);
        map_ioapic_in_current_vas((uint64_t)ioapic, PG_SUPERVISOR, PG_READ_WRITE);
        uint32_t max_gsi = ioapic_get_max_redirection_entry(ioapic) + entry->gsi_base;
        if (ps2_1_gsi <= max_gsi)
            return true;
    }
    return false;
}

bool is_madt_entry_irq_12_capable(struct madt_entry_header* header)
{
    if (header->entry_type == 1)    // * I/O APIC
    {
        struct madt_ioapic_entry* entry = (struct madt_ioapic_entry*)header;
        if (entry->gsi_base > ps2_12_gsi)
            return false;
        volatile io_apic_registers_t* ioapic = (volatile io_apic_registers_t*)((uint64_t)entry->ioapic_address);
        map_ioapic_in_current_vas((uint64_t)ioapic, PG_SUPERVISOR, PG_READ_WRITE);
        uint32_t max_gsi = ioapic_get_max_redirection_entry(ioapic) + entry->gsi_base;
        if (ps2_12_gsi <= max_gsi)
            return true;
    }
    return false;
}

void madt_extract_data()
{
    if (!madt) return;

    LOG(DEBUG, "Extracting data from the MADT");

    if (ps2_controller_connected)
    {
        struct madt_entry_header* ps2_irq_source_1 = find_entry_in_madt(is_madt_entry_irq_source_1);
        struct madt_entry_header* ps2_irq_source_12 = find_entry_in_madt(is_madt_entry_irq_source_12);

        ps2_1_gsi = 1;
        ps2_12_gsi = 12;

        // * Override
        if (ps2_irq_source_1)
            ps2_1_gsi = ((struct madt_ioapic_interrupt_source_override_entry*)ps2_irq_source_1)->gsi;
        if (ps2_irq_source_12)
            ps2_12_gsi = ((struct madt_ioapic_interrupt_source_override_entry*)ps2_irq_source_12)->gsi;

        LOG(DEBUG, "PS/2 IRQ 1 GSI: %u", ps2_1_gsi);
        LOG(DEBUG, "PS/2 IRQ 12 GSI: %u", ps2_12_gsi);

        struct madt_ioapic_entry* ps2_1_ioapic_entry = (struct madt_ioapic_entry*)find_entry_in_madt(is_madt_entry_irq_1_capable);
        struct madt_ioapic_entry* ps2_12_ioapic_entry = (struct madt_ioapic_entry*)find_entry_in_madt(is_madt_entry_irq_12_capable);

        if (ps2_1_ioapic_entry)
        {
            LOG(DEBUG, "Found I/O APIC entry able to handle GSI %u", ps2_1_gsi);
            volatile io_apic_registers_t* ps2_1_ioapic = (volatile io_apic_registers_t*)(uint64_t)ps2_1_ioapic_entry->ioapic_address;
            uint64_t redirection_entry = ioapic_read_redirection_entry(ps2_1_ioapic, ps2_1_gsi - ps2_1_ioapic_entry->gsi_base);
            ioapic_write_redirection_entry(ps2_1_ioapic, ps2_1_gsi - ps2_1_ioapic_entry->gsi_base, 
                (redirection_entry & (0x00FFFFFFFFFE0000)) |
                APIC_PS2_1_INT |
                APIC_DELIVERY_FIXED |
                APIC_DESTINATION_PHYSICAL |
                APIC_POLARITY_ACTIVE_HIGH |
                APIC_TRIGGER_EDGE |
                APIC_MASK_ENABLED |
                ((uint64_t)apic_get_cpu_id() << 56));
        }
        if (ps2_12_ioapic_entry)
        {
            LOG(DEBUG, "Found I/O APIC entry able to handle GSI %u", ps2_12_gsi);
            volatile io_apic_registers_t* ps2_12_ioapic = (volatile io_apic_registers_t*)(uint64_t)ps2_12_ioapic_entry->ioapic_address;
            uint64_t redirection_entry = ioapic_read_redirection_entry(ps2_12_ioapic, ps2_12_gsi - ps2_12_ioapic_entry->gsi_base);
            ioapic_write_redirection_entry(ps2_12_ioapic, ps2_12_gsi - ps2_12_ioapic_entry->gsi_base, 
                (redirection_entry & (0x00FFFFFFFFFE0000)) |
                APIC_PS2_2_INT |
                APIC_DELIVERY_FIXED |
                APIC_DESTINATION_PHYSICAL |
                APIC_POLARITY_ACTIVE_HIGH |
                APIC_TRIGGER_EDGE |
                APIC_MASK_ENABLED |
                ((uint64_t)apic_get_cpu_id() << 56));
        }
    }
}