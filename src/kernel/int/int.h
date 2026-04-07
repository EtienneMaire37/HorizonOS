#pragma once

#include <stdint.h>
#include <signal.h>
#include <stdlib.h>
#include <assert.h>
#include "../initrd/initrd.h"
#include "../util/defs.h"

typedef struct __attribute__((packed)) interrupt_registers
{
    uint64_t padding;

    uint64_t cr3, cr2;
    uint64_t ds;

    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rbp;
    uint64_t rbx;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rax;

    uint64_t interrupt_number, error_code;
    uint64_t rip, cs, rflags;

    uint64_t rsp, ss;   // * "64-bit mode also pushes SS:RSP unconditionally, rather than only on a CPL change." -- Intel manual vol 3A 7.14.2
} interrupt_registers_t;

#define DIVISION_OVERFLOW_EXCEPTION         0
#define DEBUG_EXCEPTION                     1
#define NON_MASKABLE_INTERRUPT              2
#define BREAKPOINT                          3
#define OVERFLOW                            4
#define BOUND_RANGE_EXCEEDED                5
#define INVALID_OPCODE                      6
#define DEVICE_NOT_AVAILABLE                7
#define DOUBLE_FAULT                        8
#define COPROCESSOR_SEGMENT_OVERRUN         9
#define INVALID_TSS                         10
#define SEGMENT_NOT_PRESENT                 11
#define STACK_SEGMENT_FAULT                 12
#define GENERAL_PROTECTION_FAULT            13
#define PAGE_FAULT                          14
#define X87_FLOATING_POINT_EXCEPTION        16
#define ALIGNMENT_CHECK                     17
#define MACHINE_CHECK                       18
#define SIMD_FLOATING_POINT_EXCEPTION       19
#define VIRTUALIZATION_EXCEPTION            20
#define CONTROL_PROTECTION_EXCEPTION        21
#define HYPERVISOR_INJECTION_EXCEPTION      28
#define VMM_COMMUNICATION_EXCEPTION         29
#define SECURITY_EXCEPTION                  30

static inline const char* get_error_message(uint32_t fault, uint32_t error_code)
{
    if (fault == PAGE_FAULT)
    {
        if ((error_code >> 15) & 1)
            return "SGX_VIOLATION_EXCEPTION";
    }

    switch (fault)
    {
    def_case(DIVISION_OVERFLOW_EXCEPTION)
    def_case(DEBUG_EXCEPTION)
    def_case(NON_MASKABLE_INTERRUPT)
    def_case(BREAKPOINT)
    def_case(OVERFLOW)
    def_case(BOUND_RANGE_EXCEEDED)
    def_case(INVALID_OPCODE)
    def_case(DEVICE_NOT_AVAILABLE)
    def_case(DOUBLE_FAULT)
    def_case(COPROCESSOR_SEGMENT_OVERRUN)
    def_case(INVALID_TSS)
    def_case(SEGMENT_NOT_PRESENT)
    def_case(STACK_SEGMENT_FAULT)
    def_case(GENERAL_PROTECTION_FAULT)
    def_case(PAGE_FAULT)
    def_case(X87_FLOATING_POINT_EXCEPTION)
    def_case(ALIGNMENT_CHECK)
    def_case(MACHINE_CHECK)
    def_case(SIMD_FLOATING_POINT_EXCEPTION)
    def_case(VIRTUALIZATION_EXCEPTION)
    def_case(CONTROL_PROTECTION_EXCEPTION)
    def_case(HYPERVISOR_INJECTION_EXCEPTION)
    def_case(VMM_COMMUNICATION_EXCEPTION)
    def_case(SECURITY_EXCEPTION)
    default:
        return "";
    }
}
static inline int get_signal_from_exception(interrupt_registers_t* registers)
{
    assert(registers);

    switch (registers->interrupt_number)
    {
    case DIVISION_OVERFLOW_EXCEPTION:
    case X87_FLOATING_POINT_EXCEPTION:
    case SIMD_FLOATING_POINT_EXCEPTION:
        return SIGFPE;

    case DEBUG_EXCEPTION:
    case BREAKPOINT:
        return SIGTRAP;

    case INVALID_OPCODE:
        return SIGILL;

    case ALIGNMENT_CHECK:
        return SIGBUS;

    case BOUND_RANGE_EXCEEDED:
    case INVALID_TSS:
    case SEGMENT_NOT_PRESENT:
    case STACK_SEGMENT_FAULT:
    case GENERAL_PROTECTION_FAULT:
    case PAGE_FAULT:
    case OVERFLOW:
        return SIGSEGV;

    default:
        assert(!"Unknown exception");
    }
}

extern initrd_file_t* kernel_symbols_file;

void interrupt_handler(interrupt_registers_t* registers);
extern void intret();
