#include "signal.h"
#include "multitasking.h"
#include "syscall.h"
#include "../util/memory.h"

void setup_user_signal_stack_frame__interrupt(interrupt_registers_t* registers)
{
    assert(registers);

    uint64_t ret_rsp = registers->rsp;
    ret_rsp -= 128; // * skip red zone
    ret_rsp &= ~0xf;    // * Align according to SysV ABI
    ret_rsp -= sizeof(interrupt_registers_t);

    // LOG(TRACE, "pushed ret rsp: %#16" PRIx64, ret_rsp);

    memcpy((void*)ret_rsp, registers, sizeof(interrupt_registers_t));
    registers->rip = (uint64_t)sighandler;
    registers->rsp = ret_rsp;
    registers->rax = current_task->pending_signal_handler;

    // * Arguments
    registers->rdi = current_task->pending_signal_number;
    registers->rsi = 0;
    registers->rdx = 0;

    // hexdump((void*)ret_rsp, sizeof(interrupt_registers_t));
}
