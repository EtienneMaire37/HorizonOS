#include "timer.h"

#include "../time/time.h"
#include "../time/ktime.h"
#include "../cmos/cmos.h"
#include "../cpu/tsc.h"
#include "../cpu/util.h"
#include "../util/vector.h"
#include "../multicore/spinlock.h"

#include <sys/types.h>
#include <string.h>

DEFINE_VECTOR(u64, uint64_t)

vector_u64_t tsc_deadlines = vector_u64_init;
spinlock_noint_t tsc_deadlines_lock = SPINLOCK_NOINT_INIT;
uint64_t target_deadline = 0;

void apic_timer_and_tsc_init()
{
    try_calibrate_tsc_with_cpuid();

    rtc_wait_while_updating();

    uint64_t start_tsc = rdtsc();

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

    uint64_t tsc_per_second = rdtsc() - start_tsc;

    if (!tsc_cycles_per_second)
        tsc_cycles_per_second = tsc_per_second;

    {
        uint32_t eax, ebx, ecx, edx;
        cpuid(1, eax, ebx, ecx, edx);
        ASSERT(ecx & (1 << 24)); // * Check for TSC-deadline
    }

    if (lapic)
        lapic->lvt_timer_register = APIC_TIMER_INT | LAPIC_TIMER_TSC_DEADLINE;
    else
        wrmsr(IA32_X2APIC_LVT_TIMER_MSR, APIC_TIMER_INT | LAPIC_TIMER_TSC_DEADLINE);
}

void __apic_timer_set_next_deadline(uint64_t tsc_deadline)
{
    if (lapic) mfence();
    target_deadline = tsc_deadline;
    wrmsr(IA32_TSC_DEADLINE_MSR, tsc_deadline);
}

void apic_timer_set_next_deadline(uint64_t tsc_deadline)
{
    uint32_t flags = acquire_spinlock_noint(&tsc_deadlines_lock);
    __apic_timer_set_next_deadline(tsc_deadline);
    release_spinlock_noint(&tsc_deadlines_lock, flags);
}

void apic_timer_add_new_deadline(uint64_t tsc_deadline)
{
    if (tsc_deadline == 0) return;
    uint32_t flags = acquire_spinlock_noint(&tsc_deadlines_lock);
    size_t len = vector_u64_size(&tsc_deadlines);
    if (len == 0 || tsc_deadline < target_deadline)
        vector_u64_push_back(&tsc_deadlines, tsc_deadline);
    else
    {
        if (tsc_deadline > *vector_u64_at(&tsc_deadlines, 0))
        {
            vector_u64_push_back(&tsc_deadlines, 0);
            memmove((uint8_t*)tsc_deadlines.data + sizeof(uint64_t), tsc_deadlines.data, len * sizeof(uint64_t));
            *vector_u64_at(&tsc_deadlines, 0) = tsc_deadline;
        }
        else
        {
            for (ssize_t i = len - 1; i >= 0; i--)
            {
                uint64_t cur = *vector_u64_at(&tsc_deadlines, i);
                if (tsc_deadline <= cur)
                {
                    if (cur == tsc_deadline)
                    {
                        release_spinlock_noint(&tsc_deadlines_lock, flags);
                        return;
                    }
                    vector_u64_push_back(&tsc_deadlines, 0);
                    memmove((uint8_t*)tsc_deadlines.data + sizeof(uint64_t) * (i + 2), tsc_deadlines.data + sizeof(uint64_t) * (i + 1), (len - i - 1) * sizeof(uint64_t));
                    *vector_u64_at(&tsc_deadlines, i + 1) = tsc_deadline;
                    break;
                }
            }
        }
    }
    uint64_t closest_deadline = *vector_u64_at(&tsc_deadlines, len);
    if (len == 0 || target_deadline > closest_deadline)
        __apic_timer_set_next_deadline(closest_deadline);
    release_spinlock_noint(&tsc_deadlines_lock, flags);
}

void apic_timer_handle_irq()
{
    uint32_t flags = acquire_spinlock_noint(&tsc_deadlines_lock);
    const uint64_t tsc = rdtsc();
    size_t len;
    while (true)
    {
        len = vector_u64_size(&tsc_deadlines);
        if (len == 0)
            break;
        uint64_t new_deadline = *vector_u64_at(&tsc_deadlines, len - 1);
        vector_u64_pop_back(&tsc_deadlines);
        if (new_deadline > tsc)
        {
            __apic_timer_set_next_deadline(new_deadline);
            break;
        }
    }
    release_spinlock_noint(&tsc_deadlines_lock, flags);
    flags = acquire_spinlock_noint(&time_lock);
    __resolve_time();
    release_spinlock_noint(&time_lock, flags);
}
