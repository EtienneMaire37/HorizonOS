#include "defs.h"
#include "../util/lambda.h"
#include "../multicore/spinlock.h"
#include "../debug/out.h"

#include <assert.h>

// * page 0xFEE00xxx
volatile local_apic_registers_t* lapic = NULL;
spinlock_noint_t ioapic_lock = SPINLOCK_NOINT_INIT; // TODO: Implement a per I/O APIC lock

#include "apic.h"
#include "../acpi/tables.h"
#include "../paging/paging.h"
#include "../cpu/memory.h"
#include "../cpu/msr.h"
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

        uint32_t flags = acquire_spinlock_noint(&vmm_lock);
        lapic = __vmm_find_free_kernel_space_pages(NULL, 1);
        LOG(DEBUG, "Mapping local APIC at physical address %#" PRIx64 " to %p", paddr, lapic);

        remap_range((uint64_t*)(get_cr3_address() + PHYS_MAP_BASE),
            (uint64_t)lapic, paddr,
            1, PG_SUPERVISOR, PG_READ_WRITE, CACHE_UC);
        release_spinlock_noint(&vmm_lock, flags);
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
    if (lapic)  lapic->spurious_interrupt_vector_register = lapic->spurious_interrupt_vector_register | 0x100;
    else        wrmsr(IA32_X2APIC_SIVR_MSR, rdmsr(IA32_X2APIC_SIVR_MSR) | 0x100);
}

void lapic_disable()
{
    if (lapic)  lapic->spurious_interrupt_vector_register = (lapic->spurious_interrupt_vector_register & ~0x100);
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
    uint32_t flags = acquire_spinlock_noint(&ioapic_lock);
    ioapic->IOREGSEL = reg;
    uint32_t ret = ioapic->IOWIN;
    release_spinlock_noint(&ioapic_lock, flags);
    return ret;
}

void ioapic_write_register(volatile io_apic_registers_t* ioapic, uint8_t reg, uint32_t val)
{
    uint32_t flags = acquire_spinlock_noint(&ioapic_lock);
    ioapic->IOREGSEL = reg;
    ioapic->IOWIN = val;
    release_spinlock_noint(&ioapic_lock, flags);
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
    uint32_t flags = acquire_spinlock_noint(&vmm_lock);
    void* vaddr = __vmm_find_free_kernel_space_pages(NULL, 1);

    LOG(TRACE, "Mapping I/O APIC at physical address %#" PRIx64 " to %p", paddr, vaddr);

    remap_range((uint64_t*)(get_cr3_address() + PHYS_MAP_BASE),
        (uint64_t)vaddr, paddr,
        1, PG_SUPERVISOR, PG_READ_WRITE, CACHE_UC);
    release_spinlock_noint(&vmm_lock, flags);
    invlpg((uint64_t)vaddr);

    return vaddr;
}

void unmap_ioapic(void* addr)
{
    LOG(TRACE, "Unmapping I/O APIC at virtual address %p", addr);

    unmap_range((uint64_t*)(get_cr3_address() + PHYS_MAP_BASE),
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

void apic_map_irq_from_source(int irq_number, int isr_number)
{
    ASSERT(madt);

    LOG(DEBUG, "Mapping IRQ %d to ISR %d", irq_number, isr_number);

    struct madt_entry_header* irq_source_entry = find_entry_in_madt(lambda(bool, (struct madt_entry_header* header)
    {
        if (header->entry_type == 2)    // * I/O APIC Interrupt Source Override
        {
            struct madt_ioapic_interrupt_source_override_entry* entry = (struct madt_ioapic_interrupt_source_override_entry*)header;
            if (entry->irq_source == irq_number)
                return true;
        }
        return false;
    }
    ));

    uint32_t gsi = irq_source_entry ? ((struct madt_ioapic_interrupt_source_override_entry*)irq_source_entry)->gsi : (uint32_t)irq_number;

    LOG(DEBUG, "GSI for IRQ source %d is %u", irq_number, gsi);

    struct madt_ioapic_entry* ioapic_entry = (struct madt_ioapic_entry*)find_entry_in_madt(lambda(bool, (struct madt_entry_header* header)
    {
        if (header->entry_type == 1)    // * I/O APIC
        {
            struct madt_ioapic_entry* entry = (struct madt_ioapic_entry*)header;
            if (entry->gsi_base > gsi)
                return false;
            volatile io_apic_registers_t* ioapic = map_ioapic_in_current_vas(entry->ioapic_address);
            uint32_t max_gsi = ioapic_get_max_redirection_entry(ioapic) + entry->gsi_base;
            unmap_ioapic((void*)ioapic);
            if (gsi <= max_gsi)
                return true;
        }
        return false;
    }));

    if (!ioapic_entry)
    {
        LOG(ERROR, "No I/O APIC can handle GSI %d", gsi);
        return;
    }

    uint32_t lapic_id = lapic_get_cpu_id();
    // ! Horrible way to do things
    // TODO: Use logical destination mode
    ASSERT(lapic_id == (lapic_id & 0xff));
    volatile io_apic_registers_t* ioapic = map_ioapic_in_current_vas(ioapic_entry->ioapic_address);
    uint64_t redirection_entry = ioapic_read_redirection_entry(ioapic, gsi - ioapic_entry->gsi_base);
    ioapic_write_redirection_entry(ioapic, gsi - ioapic_entry->gsi_base,
        (redirection_entry & (0x00FFFFFFFFFE0000)) |
        APIC_PS2_1_INT |
        APIC_DELIVERY_FIXED |
        APIC_DESTINATION_PHYSICAL |
        APIC_POLARITY_ACTIVE_HIGH |
        APIC_TRIGGER_EDGE |
        APIC_MASK_ENABLED |
        ((uint64_t)lapic_id << 56));
    unmap_ioapic((void*)ioapic);

    LOG(DEBUG, "Mapped GSI %d to ISR %d", gsi, isr_number);
}
