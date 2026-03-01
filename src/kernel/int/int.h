#pragma once

#include <stdint.h>
#include "../initrd/initrd.h"

typedef struct __attribute__((packed)) interrupt_registers
{
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

static const char* error_str[32] = 
{
    "DIVISION_OVERFLOW_EXCEPTION",
    "DEBUG",
    "NON-MASKABLE_INTERRUPT",
    "BREAKPOINT",
    "OVERFLOW",
    "BOUND_RANGE_EXCEEDED",
    "INVALID_OPCODE",
    "DEVICE_NOT_AVAILABLE",
    "DOUBLE_FAULT",
    "COPROCESSOR_SEGMENT_OVERRUN",
    "INVALID_TSS",
    "SEGMENT_NOT_PRESENT",
    "STACK_SEGMENT_FAULT",
    "GENERAL_PROTECTION_FAULT",
    "PAGE_FAULT",
    "",
    "X87_FLOATING_POINT_EXCEPTION",
    "ALIGNMENT_CHECK",
    "MACHINE_CHECK",
    "SIMD_FLOATING_POINT_EXCEPTION",
    "VIRTUALIZATION_EXCEPTION",
    "CONTROL_PROTECTION_EXCEPTION",
    "",
    "",
    "",
    "",
    "",
    "",
    "HYPERVISOR_INJECTION_EXCEPTION",
    "VMM_COMMUNICATION_EXCEPTION",
    "SECURITY_EXCEPTION",
    ""
};

// char* seg_error_code[4] = 
// {
//     "GDT",
//     "IDT",
//     "LDT",
//     "IDT"
// };

static inline const char* get_error_message(uint32_t fault, uint32_t error_code)
{
    if (fault >= 32) return "";
    if (fault == 14)    // Page fault
    {
        if ((error_code >> 15) & 1)
            return "SGX_VIOLATION_EXCEPTION";
    }
    return error_str[fault];
}

extern initrd_file_t* kernel_symbols_file;

void interrupt_handler(interrupt_registers_t* registers);
extern void intret();