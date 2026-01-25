#pragma once

#include "../../libc/include/stdint.h"
#include "msr.h"

typedef struct __attribute__((packed)) 
{
    uint64_t user_rsp;
    uint64_t kernel_rsp;
} syscall_data_t;

extern syscall_data_t sc_data;

// TODO: Check cpuid (EAX=7, ECX=0 -> EBX bit 0)

static void wrfsbase(uint64_t address)
{
    // asm volatile("wrfsbase rax" :: "a"(address));

    wrmsr(IA32_FS_BASE_MSR, address);
}

static void wrgsbase(uint64_t address)
{
    // asm volatile("wrgsbase rax" :: "a"(address));

    wrmsr(IA32_GS_BASE_MSR, address);
}