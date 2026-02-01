#include "../initrd/initrd.h"
#include <stddef.h>

initrd_file_t* kernel_symbols_file = NULL;

#include <sys/wait.h>

#include "../multitasking/vas.h"
#include "../multitasking/syscall.h"
#include "int.h"
#include "irq.h"
#include "nmi.h"

#include "kernel_panic.h"

#define return_from_isr() return

void interrupt_handler(interrupt_registers_t* registers)
{
    // LOG(TRACE, "Interrupt %" PRIu64, registers->interrupt_number);
    if (registers->interrupt_number == 2)       // * NMI
    {
        LOG(TRACE, "NMI: %#x, %#x", inb(SYSTEM_CONTROL_PORT_A), inb(SYSTEM_CONTROL_PORT_B));

        return_from_isr();
    }
    if (registers->interrupt_number < 32)       // * Fault
    {
        LOG(WARNING, multitasking_enabled ? "[task \"%s\" (pid %u)]: " : "", __CURRENT_TASK.name, __CURRENT_TASK.pid);
        CONTINUE_LOG(WARNING, "Fault : Exception number : %" PRIu64 " ; Error : %s ; Error code = %#" PRIx64 " ; cr2 = %#" PRIx64 " ; cr3 = %#" PRIx64 " ; rip = %#" PRIx64, 
            registers->interrupt_number, get_error_message(registers->interrupt_number, registers->error_code), 
            registers->error_code, registers->cr2, registers->cr3, registers->rip);
        
        LOG(WARNING, "CS: %#.16" PRIx64 " DS: %#.16" PRIx64 " SS: %#.16" PRIx64, registers->cs, registers->ds, registers->ss);

        if (__CURRENT_TASK.system_task || task_count == 1 || !multitasking_enabled || registers->interrupt_number == 8 || registers->interrupt_number == 18)
        // System task or last task or multitasking not enabled or Double Fault or Machine Check
        {
            disable_interrupts();
            kernel_panic((interrupt_registers_t*)registers);
        }
        else
        {
            log_registers();
            __CURRENT_TASK.return_value = registers->interrupt_number == 14 ? SIGSEGV : SIGILL;
            __CURRENT_TASK.is_dead = true;
            switch_task();
        }

        return_from_isr();
    }

    if (registers->interrupt_number < 32 + 16)  // * IRQ
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
