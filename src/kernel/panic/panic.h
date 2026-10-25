#pragma once

#include "../debug/out.h"
#include "../int/int.h"
#include "../cpu/segbase.h"
#include "../util/defs.h"

#define PANIC_INVALID           0
#define PANIC_DEBUG             1
#define PANIC_STCK_CHK_FAIL     2
#define PANIC_OUT_OF_MEMORY     3

static inline const char* get_panic_message(int err)
{
    switch (err)
    {
    def_case(PANIC_INVALID)
    def_case(PANIC_DEBUG)
    def_case(PANIC_STCK_CHK_FAIL)
    def_case(PANIC_OUT_OF_MEMORY)
    default:
        return "PANIC_INVALID";
    }
}

#define log_registers() do { LOG(INFO, "RSP=%#016" PRIx64 " RBP=%#016" PRIx64 " RAX=%#016" PRIx64 " RBX=%#016" PRIx64 " RCX=%#016" PRIx64 " RDX=%#016" PRIx64, \
    registers->rsp, registers->rbp, registers->rax, registers->rbx, registers->rcx, registers->rdx);    \
    LOG(INFO, "R8=%#016" PRIx64 " R9=%#016" PRIx64 " R10=%#016" PRIx64 " R11=%#016" PRIx64 " R12=%#016" PRIx64 " R13=%#016" PRIx64 " R14=%#016" PRIx64 " R15=%#016" PRIx64,  \
    registers->r8, registers->r9, registers->r10, registers->r11, registers->r12, registers->r13, registers->r14, registers->r15);  \
    LOG(INFO, "RDI=%#016" PRIx64 " RSI=%#016" PRIx64, registers->rdi, registers->rsi); \
    log_segbase(); } while (0)

#define is_a_valid_function(symbol_type) ((symbol_type) == 'T' || (symbol_type) == 'R' || (symbol_type) == 't' || (symbol_type) == 'r')

void print_kernel_symbol_name(uintptr_t rip, bool ttyout);
void print_stack_trace(uint64_t rip, uint64_t rbp_val, bool ttyout);
void __attribute__((noreturn)) kernel_panic_ex(interrupt_registers_t* registers, int error);

static inline void __attribute__((noreturn)) kernel_panic(interrupt_registers_t* registers)
{
    kernel_panic_ex(registers, 0);
}
