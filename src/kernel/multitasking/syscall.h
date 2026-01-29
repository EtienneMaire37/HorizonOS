#pragma once

#include "../int/int.h"
#include "../../libc/src/syscall_defines.h"
#include "startup_data.h"
#include "../multitasking/loader.h"
#include <signal.h>
#include "../../libc/include/sys/wait.h"
#include "../../libc/src/math_utils.h"
#include "../../libc/include/fcntl.h"
#include "../vga/textio.h"
#include "../../libc/include/errno.h"
#include "../io/keyboard.h"
#include "../multitasking/task.h"
#include "../paging/paging.h"

typedef struct __attribute__((packed))
{
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
} syscall_registers_t;

#define SC_ARG1(t1) \
    t1 arg1 = (t1)registers->rbx;
#define SC_ARG2(t1, t2) \
    SC_ARG1(t1) \
    t2 arg2 = (t2)registers->rdx;
#define SC_ARG3(t1, t2, t3) \
    SC_ARG2(t1, t2) \
    t3 arg3 = (t3)registers->rsi;
#define SC_ARG4(t1, t2, t3, t4) \
    SC_ARG3(t1, t2, t3) \
    t4 arg4 = (t4)registers->rdi;
#define SC_ARG5(t1, t2, t3, t4, t5) \
    SC_ARG4(t1, t2, t3, t4) \
    t5 arg5 = (t5)registers->r8;
#define SC_ARG6(t1, t2, t3, t4, t5, t6) \
    SC_ARG5(t1, t2, t3, t4, t5) \
    t6 arg6 = (t6)registers->r9;

#define SC_ARGS_SELECT(count, ...) SC_ARG##count(__VA_ARGS__)

#define sc_case(syscall_id, argc, ...) } \
    case syscall_id: \
    { \
        SC_ARGS_SELECT(argc, __VA_ARGS__)

#define SC_RET0         registers->rax
#define SC_RET1         registers->rbx

#define sc_ret(n)       SC_RET##n
#define sc_ret_errno    sc_ret(0)

#ifdef LOG_SYSCALLS
#define SC_LOG(fmt, ...) LOG(TRACE, fmt, __VA_ARGS__)
#else
#define SC_LOG(fmt, ...)
#endif

void c_syscall_handler(syscall_registers_t* registers);

void handle_syscall(interrupt_registers_t* registers);