#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "msr.h"

typedef struct __attribute__((packed)) 
{
    uint64_t user_rsp;
    uint64_t kernel_rsp;
} syscall_data_t;

extern syscall_data_t sc_data;
extern bool fsgsbase;

static void wrfsbase(uint64_t address)
{
    if (fsgsbase)
        asm volatile("wrfsbase rax" :: "a"(address));
    else
        wrmsr(IA32_FS_BASE_MSR, address);
}

static void wrgsbase(uint64_t address)
{
    if (fsgsbase)
        asm volatile("wrgsbase rax" :: "a"(address));
    else
        wrmsr(IA32_GS_BASE_MSR, address);
}

static uint64_t rdfsbase()
{
    if (fsgsbase)
    {
        uint64_t address;
        asm volatile("rdfsbase rax" : "=a"(address));
        return address;
    }
    return rdmsr(IA32_FS_BASE_MSR);
}

static uint64_t rdgsbase()
{
    if (fsgsbase)
    {
        uint64_t address;
        asm volatile("rdgsbase rax" : "=a"(address));
        return address;
    }
    return rdmsr(IA32_GS_BASE_MSR);
}

#define log_segbase()   LOG(INFO, "FS BASE: %#.16" PRIx64 " GS BASE: %#.16" PRIx64 " KERNEL GS BASE: %#.16" PRIx64, rdfsbase(), rdgsbase(), rdmsr(IA32_KERNEL_GS_BASE_MSR))