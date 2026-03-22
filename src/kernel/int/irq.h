#pragma once

#include "../ps2/ps2.h"
#include "int.h"
#include "../pic/apic.h"
#include "../time/ktime.h"
#include "../terminal/textio.h"
#include "../multitasking/multitasking.h"
#include "../multitasking/queue.h"
#include "../util/lambda.h"

void handle_apic_irq(interrupt_registers_t* registers)
{
    lock_scheduler();
    bool ts = false, sigint = false;
    switch (registers->interrupt_number)
    {
    case APIC_TIMER_INT:
    {
        uint64_t increment = precise_time_to_milliseconds(GLOBAL_TIMER_INCREMENT);
        global_timer += GLOBAL_TIMER_INCREMENT;
        system_thousands += increment;

        if (current_task && multitasking_enabled)
            current_task->current_cpu_ticks += increment;
        if (system_thousands >= 1000 && multitasking_enabled)
        {
            lock_scheduler();
            thread_t* cur = running_tasks;
            do
            {
                cur->stored_cpu_ticks = cur->current_cpu_ticks;
                cur->current_cpu_ticks = 0;

                cur = cur->next;
            } while (cur != running_tasks);
            unlock_scheduler();
        }

        resolve_time();

        #ifdef TTY_CURSOR_BLINK
        if (system_thousands - increment < 500 && system_thousands >= 500)
        {
            tty_cursor_blink ^= true;
            if (tty_cursor_blink)
            {
                tty_render_cursor(tty_cursor);
            }
            else
            {
                uint16_t data = tty_data[tty_cursor];
                tty_render_character(tty_cursor, data, data >> 8);
            }
        }
        #endif

        if (multitasking_enabled)
            filter_tasks_to_running_queue(&waiting_for_time_tasks, lambda(bool, (thread_t* task){ return task->timeout_deadline < global_timer; }));

        if (multitasking_enabled)
        {
            if (multitasking_counter <= 0)
            {
                multitasking_counter = TASK_SWITCH_DELAY;

                ts = true;
            }
            multitasking_counter -= precise_time_to_milliseconds(GLOBAL_TIMER_INCREMENT);
        }
        break;
    }

    case APIC_PS2_1_INT:
        handle_irq_1(&ts, &sigint);
        break;

    case APIC_PS2_2_INT:
        handle_irq_12(&ts, &sigint);
        break;

    default:    // * Spurious interrupt
        return;
    }

    lapic_send_eoi();

    if (sigint)
        task_send_signal_to_pgrp(SIGINT, tty_foreground_pgrp);

    if (ts)
        switch_task();

    unlock_scheduler();
}
