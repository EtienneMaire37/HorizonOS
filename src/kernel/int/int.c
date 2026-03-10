#include "../initrd/initrd.h"
#include <stddef.h>

initrd_file_t* kernel_symbols_file = NULL;

#include <sys/wait.h>

#include "../multitasking/vas.h"
#include "../multitasking/syscall.h"
#include "int.h"
#include "irq.h"
#include "nmi.h"
#include "../multitasking/signal.h"

#include "kernel_panic.h"

#define return_from_isr() { if (multitasking_enabled) { lock_scheduler(); if (current_task->sig_pending_user_space && registers->cs != KERNEL_CODE_SEGMENT) { setup_user_signal_stack_frame__interrupt(registers); current_task->sig_pending_user_space = false; } unlock_scheduler(); } return; }

void interrupt_handler(interrupt_registers_t* registers)
{
    if (registers->interrupt_number == NON_MASKABLE_INTERRUPT)
    {
        LOG(TRACE, "NMI: %#x, %#x", inb(SYSTEM_CONTROL_PORT_A), inb(SYSTEM_CONTROL_PORT_B));

        return_from_isr();
    }
    if (registers->interrupt_number < 32)       // * Fault
    {
        if (multitasking_enabled)
            LOG(WARNING, "[task \"%s\" (pid %u)]: ", current_task->name, current_task->pid);
        
        CONTINUE_LOG(WARNING, "Fault : Exception number : %" PRIu64 " ; Error : %s ; Error code = %#" PRIx64 " ; cr2 = %#" PRIx64 " ; cr3 = %#" PRIx64 " ; rip = %#" PRIx64, 
            registers->interrupt_number, get_error_message(registers->interrupt_number, registers->error_code), 
            registers->error_code, registers->cr2, registers->cr3, registers->rip);
        
        LOG(WARNING, "CS: %#.16" PRIx64 " DS: %#.16" PRIx64 " SS: %#.16" PRIx64, registers->cs, registers->ds, registers->ss);
        log_segbase();

        if (multitasking_enabled)
        {
            if (current_task->system_task || task_count == 1 || !multitasking_enabled || 
registers->interrupt_number == DOUBLE_FAULT || registers->interrupt_number == MACHINE_CHECK)
            {
                disable_interrupts();
                kernel_panic((interrupt_registers_t*)registers);
            }
            else
            {
                int signum = get_signal_from_exception(registers);
                task_send_signal(current_task, signum);
                kill_task(current_task, signum);
                abort();
            }
        }
        else
            kernel_panic((interrupt_registers_t*)registers);
        return_from_isr();
    }

    if (registers->interrupt_number < 32 + 16)  // * PIC IRQs
    {
        // uint8_t irq_number = registers->interrupt_number - 32;

        // if (irq_number == 7 && !(pic_get_isr() >> 7))
        //     return_from_isr();
        // if (irq_number == 15 && !(pic_get_isr() >> 15))
        // {
        //     outb(PIC1_CMD, PIC_EOI);
	    //     io_wait();
        //     return_from_isr();
        // }

        // pic_send_eoi(irq_number);

        // * Already disabled the PIC so only spurious interrupts can happen

        return_from_isr();
    }

    // * APIC interrupt (probably)

    handle_apic_irq(registers);

    return_from_isr();
}
