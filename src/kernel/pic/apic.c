#include "defs.h"
#include "../util/lambda.h"

#include <assert.h>

// * page 0xFEE00xxx
volatile local_apic_registers_t* lapic = NULL;

uint32_t ps2_1_gsi = 1, ps2_12_gsi = 12;

#include "apic.h"
#include "../acpi/tables.h"
#include "../paging/paging.h"
#include "../cpu/memory.h"
#include "../ps2/ps2.h"
#include "../cpu/msr.h"
#include "../int/kernel_panic.h"
#include "../cmos/rtc.h"
#include "../memalloc/virtual_memory_allocator.h"

void lapic_init()
{
    uint64_t apic_base_msr = rdmsr(IA32_APIC_BASE_MSR);
    // * x2APIC enabled => CPUID.01H:ECX[21] = 1 so no need to check it
    if (apic_base_msr & (1ULL << 10))   // * x2APIC
        lapic = NULL;
    else
    {
        uint64_t paddr = (apic_base_msr & ~0xfff) & ((1ULL << physical_address_width) - 1);

        lapic = vmm_find_free_kernel_space_pages(NULL, 1);
        LOG(DEBUG, "Mapping local APIC at physical address %#" PRIx64 " to %p", paddr, lapic);

        lock_scheduler();
        remap_range((uint64_t*)(get_cr3_address() + PHYS_MAP_BASE),
            (uint64_t)lapic, paddr,
            1, PG_SUPERVISOR, PG_READ_WRITE, CACHE_UC);
        unlock_scheduler();
        invlpg((uint64_t)lapic);
    }

    apic_base_msr |= (1ULL << 11);
    wrmsr(IA32_APIC_BASE_MSR, apic_base_msr);
}

uint32_t lapic_get_cpu_id()
{
    if (lapic)
        return (lapic->id_register >> 24) & 0xff;
    else
        return rdmsr(IA32_X2APIC_APICID_MSR);
}

void lapic_send_eoi()
{
    if (lapic)
        lapic->end_of_interrupt_register = 0;
    else
        wrmsr(IA32_X2APIC_EOI_MSR, 0);
}

void lapic_set_spurious_interrupt_number(uint8_t int_num)
{
    uint32_t val = lapic ? lapic->spurious_interrupt_vector_register : rdmsr(IA32_X2APIC_SIVR_MSR);
    val &= 0xffffff00;
    val |= int_num;
    if (lapic)  lapic->spurious_interrupt_vector_register = val;
    else        wrmsr(IA32_X2APIC_SIVR_MSR, val);
}

void lapic_enable()
{
    if (lapic)  lapic->spurious_interrupt_vector_register |= 0x100;
    else        wrmsr(IA32_X2APIC_SIVR_MSR, rdmsr(IA32_X2APIC_SIVR_MSR) | 0x100);
}

void lapic_disable()
{
    if (lapic)  lapic->spurious_interrupt_vector_register &= ~0x100;
    else        wrmsr(IA32_X2APIC_SIVR_MSR, rdmsr(IA32_X2APIC_SIVR_MSR) & ~0x100);
}

void lapic_set_tpr(uint8_t p)
{
    uint32_t val = lapic ? lapic->task_priority_register : rdmsr(IA32_X2APIC_TPR_MSR);
    val &= 0xffffff00;
    val |= p;
    if (lapic)  lapic->task_priority_register = val;
    else        wrmsr(IA32_X2APIC_TPR_MSR, val);
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

void* map_ioapic_in_current_vas(uint64_t paddr)
{
    void* vaddr = vmm_find_free_kernel_space_pages(NULL, 1);

    LOG(DEBUG, "Mapping I/O APIC at physical address %#" PRIx64 " to %p", paddr, vaddr);

    lock_scheduler();
    remap_range((uint64_t*)(get_cr3_address() + PHYS_MAP_BASE),
        (uint64_t)vaddr, paddr,
        1, PG_SUPERVISOR, PG_READ_WRITE, CACHE_UC);
    unlock_scheduler();
    invlpg((uint64_t)vaddr);

    return vaddr;
}

void unmap_ioapic(void* addr)
{
    LOG(DEBUG, "Unmapping I/O APIC at virtual address %p", addr);

    free_range((uint64_t*)(get_cr3_address() + PHYS_MAP_BASE),
        (uint64_t)addr, 1);
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

void madt_extract_data()
{
    if (!madt) return;

    LOG(DEBUG, "Extracting data from the MADT");

    if (ps2_controller_connected)
    {
        struct madt_entry_header* ps2_irq_source_1 = find_entry_in_madt(lambda(bool, (struct madt_entry_header* header)
        {
            if (header->entry_type == 2)    // * I/O APIC Interrupt Source Override
            {
                struct madt_ioapic_interrupt_source_override_entry* entry = (struct madt_ioapic_interrupt_source_override_entry*)header;
                if (entry->irq_source == 1)
                    return true;
            }
            return false;
        }
        ));
        struct madt_entry_header* ps2_irq_source_12 = find_entry_in_madt(lambda(bool, (struct madt_entry_header* header)
        {
            if (header->entry_type == 2)    // * I/O APIC Interrupt Source Override
            {
                struct madt_ioapic_interrupt_source_override_entry* entry = (struct madt_ioapic_interrupt_source_override_entry*)header;
                if (entry->irq_source == 12)
                    return true;
            }
            return false;
        }
        ));

        ps2_1_gsi = 1;
        ps2_12_gsi = 12;

        // * Override
        if (ps2_irq_source_1)
            ps2_1_gsi = ((struct madt_ioapic_interrupt_source_override_entry*)ps2_irq_source_1)->gsi;
        if (ps2_irq_source_12)
            ps2_12_gsi = ((struct madt_ioapic_interrupt_source_override_entry*)ps2_irq_source_12)->gsi;

        LOG(DEBUG, "PS/2 IRQ 1 GSI: %u", ps2_1_gsi);
        LOG(DEBUG, "PS/2 IRQ 12 GSI: %u", ps2_12_gsi);

        struct madt_ioapic_entry* ps2_1_ioapic_entry = (struct madt_ioapic_entry*)find_entry_in_madt(lambda(bool, (struct madt_entry_header* header)
        {
            if (header->entry_type == 1)    // * I/O APIC
            {
                struct madt_ioapic_entry* entry = (struct madt_ioapic_entry*)header;
                if (entry->gsi_base > ps2_1_gsi)
                    return false;
                volatile io_apic_registers_t* ioapic = map_ioapic_in_current_vas(entry->ioapic_address);
                uint32_t max_gsi = ioapic_get_max_redirection_entry(ioapic) + entry->gsi_base;
                unmap_ioapic((void*)ioapic);
                if (ps2_1_gsi <= max_gsi)
                    return true;
            }
            return false;
        }));
        struct madt_ioapic_entry* ps2_12_ioapic_entry = (struct madt_ioapic_entry*)find_entry_in_madt(lambda(bool, (struct madt_entry_header* header)
        {
            if (header->entry_type == 1)    // * I/O APIC
            {
                struct madt_ioapic_entry* entry = (struct madt_ioapic_entry*)header;
                if (entry->gsi_base > ps2_12_gsi)
                    return false;
                volatile io_apic_registers_t* ioapic = map_ioapic_in_current_vas(entry->ioapic_address);
                uint32_t max_gsi = ioapic_get_max_redirection_entry(ioapic) + entry->gsi_base;
                unmap_ioapic((void*)ioapic);
                if (ps2_12_gsi <= max_gsi)
                    return true;
            }
            return false;
        }));

        uint64_t lapic_id = lapic_get_cpu_id();
        // ! Horrible way to do things
        // TODO: Use logical destination mode
        assert(lapic_id == (lapic_id & 0xff));
        if (ps2_1_ioapic_entry)
        {
            LOG(DEBUG, "Found I/O APIC entry able to handle GSI %u", ps2_1_gsi);
            volatile io_apic_registers_t* ps2_1_ioapic = map_ioapic_in_current_vas(ps2_1_ioapic_entry->ioapic_address);
            uint64_t redirection_entry = ioapic_read_redirection_entry(ps2_1_ioapic, ps2_1_gsi - ps2_1_ioapic_entry->gsi_base);
            ioapic_write_redirection_entry(ps2_1_ioapic, ps2_1_gsi - ps2_1_ioapic_entry->gsi_base,
                (redirection_entry & (0x00FFFFFFFFFE0000)) |
                APIC_PS2_1_INT |
                APIC_DELIVERY_FIXED |
                APIC_DESTINATION_PHYSICAL |
                APIC_POLARITY_ACTIVE_HIGH |
                APIC_TRIGGER_EDGE |
                APIC_MASK_ENABLED |
                (lapic_id << 56));
            unmap_ioapic((void*)ps2_1_ioapic);
        }
        if (ps2_12_ioapic_entry)
        {
            LOG(DEBUG, "Found I/O APIC entry able to handle GSI %u", ps2_12_gsi);
            volatile io_apic_registers_t* ps2_12_ioapic = map_ioapic_in_current_vas(ps2_12_ioapic_entry->ioapic_address);
            uint64_t redirection_entry = ioapic_read_redirection_entry(ps2_12_ioapic, ps2_12_gsi - ps2_12_ioapic_entry->gsi_base);
            ioapic_write_redirection_entry(ps2_12_ioapic, ps2_12_gsi - ps2_12_ioapic_entry->gsi_base,
                (redirection_entry & (0x00FFFFFFFFFE0000)) |
                APIC_PS2_2_INT |
                APIC_DELIVERY_FIXED |
                APIC_DESTINATION_PHYSICAL |
                APIC_POLARITY_ACTIVE_HIGH |
                APIC_TRIGGER_EDGE |
                APIC_MASK_ENABLED |
                (lapic_id << 56));
            unmap_ioapic((void*)ps2_12_ioapic);
        }
    }
}

void apic_timer_init()
{
    rtc_wait_while_updating();

    if (lapic)
    {
        lapic->divide_configuration_register = LAPIC_TIMER_DIVIDE_BY_16;
        lapic->initial_count_register = 0xffffffff;
    }
    else
    {
        wrmsr(IA32_X2APIC_DIV_CONF_MSR, LAPIC_TIMER_DIVIDE_BY_16);
        wrmsr(IA32_X2APIC_INIT_COUNT_MSR, 0xffffffff);
    }

    rtc_wait_while_updating();

    if (lapic)
    {
        lapic->lvt_timer_register = LAPIC_TIMER_MASKED;

        uint32_t ticks_in_1_sec = 0xffffffff - lapic->current_count_register;

        lapic->lvt_timer_register = APIC_TIMER_INT | LAPIC_TIMER_PERIODIC;
        lapic->divide_configuration_register = LAPIC_TIMER_DIVIDE_BY_16;
        lapic->initial_count_register = ticks_in_1_sec / GLOBAL_TIMER_FREQUENCY;
    }
    else
    {
        wrmsr(IA32_X2APIC_LVT_TIMER_MSR, LAPIC_TIMER_MASKED);

        uint32_t ticks_in_1_sec = 0xffffffff - rdmsr(IA32_X2APIC_CUR_COUNT_MSR);

        wrmsr(IA32_X2APIC_LVT_TIMER_MSR, APIC_TIMER_INT | LAPIC_TIMER_PERIODIC);
        wrmsr(IA32_X2APIC_DIV_CONF_MSR, LAPIC_TIMER_DIVIDE_BY_16);
        wrmsr(IA32_X2APIC_INIT_COUNT_MSR, ticks_in_1_sec / GLOBAL_TIMER_FREQUENCY);
    }
}
