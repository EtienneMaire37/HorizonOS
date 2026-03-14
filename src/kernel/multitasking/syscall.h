#pragma once

#include "../int/int.h"
#include "startup_data.h"
#include "../multitasking/loader.h"
#include <signal.h>
#include <sys/wait.h>
#include "../util/math.h"
#include <fcntl.h>
#include "../terminal/textio.h"
#include <errno.h>
#include "../io/keyboard.h"
#include "../multitasking/task.h"
#include "../paging/paging.h"
#include "../cpu/segbase.h"

#define SC_ARG0()
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
#define SC_RET2         registers->rdx

#define sc_ret(n)       SC_RET##n
#define sc_ret_errno    sc_ret(0)

#ifdef LOG_SYSCALLS
#define SC_LOG(...) LOG(TRACE, __VA_ARGS__)
#else
#define SC_LOG(fmt, ...)
#endif

void c_syscall_handler(interrupt_registers_t* registers, void** return_address);