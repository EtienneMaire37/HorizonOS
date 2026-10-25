#include "irq.h"

#include "../ps2/ps2.h"
#include "../pic/apic.h"
#include "../terminal/textio.h"
#include "../multitasking/multitasking.h"
#include "../pic/timer.h"
#include "../cpu/tsc.h"
#include "../util/lambda.h"
#include "../multitasking/queue.h"
#include "../vfs/open_file_descriptor_vector.h"

void handle_apic_irq(interrupt_registers_t* registers)
{
    bool sigint = false;
    switch (registers->interrupt_number)
    {
    case APIC_TIMER_INT:
    {
        apic_timer_handle_irq();

        if (multitasking_enabled)
        {
            uint64_t tsc = rdtsc();
            uint32_t flags = lock_scheduler();
            __run_it_on_queue(&waiting_for_time_tasks, lambda(void, (thread_t* task)
            {
                if (task->timeout_deadline == NO_TIMEOUT)
                    return;
                if (task->timeout_deadline >= tsc)
                    return;
                __task_stop_polling(task);
            }));
            unlock_scheduler(flags);
        }
        break;
    }

    case APIC_PS2_1_INT:
        handle_ps2_irq(&sigint);
        break;

    case APIC_PS2_2_INT:
        handle_ps2_irq(&sigint);
        break;

    default:    // * Spurious interrupt
        return;
    }

    lapic_send_eoi();

    if (sigint)
        task_send_signal_to_pgrp(SIGINT, tty_foreground_pgrp);
}
