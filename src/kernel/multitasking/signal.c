#include "signal.h"
#include "multitasking.h"

void setup_user_signal_stack_frame__interrupt(interrupt_registers_t* registers)
{
    assert(registers);

    // LOG(TRACE, "setup_user_signal_stack_frame__interrupt(%p)", registers);
    // LOG(TRACE, "Pushing return address %#" PRIx64, registers->rip);
    
    uint64_t ret_rsp = registers->rsp;
    ret_rsp -= 128;  // * skip red zone
    ret_rsp -= sizeof(interrupt_registers_t);
    
    memcpy((void*)ret_rsp, registers, sizeof(interrupt_registers_t));
    registers->rip = (uint64_t)sighandler;
    registers->rsp = ret_rsp;
    registers->rax = current_task->pending_signal_handler;

    // * Arguments
    registers->rdi = current_task->pending_signal_number;
    registers->rsi = 0;
    registers->rdx = 0;
}