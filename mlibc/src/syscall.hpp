#pragma once

#include <stdint.h>

#define SYS_SETFS       0
#define SYS_WRITE       1
#define SYS_EXIT        2
#define SYS_ISATTY      3
#define SYS_VM_MAP      4
#define SYS_VM_UNMAP    5
#define SYS_SEEK        6
#define SYS_OPEN        7
#define SYS_CLOSE       8
#define SYS_READ        9
#define SYS_IOCTL       10
#define SYS_EXECVE      11
#define SYS_TCSETATTR   12
#define SYS_TCGETATTR   13
#define SYS_GETCWD      14
#define SYS_CHDIR       15
#define SYS_FORK        16
#define SYS_SIGACTION   17
#define SYS_SIGPROCMASK	18
#define SYS_WAIT4       19
#define SYS_TTYNAME     20
#define SYS_GETRESUID   21
#define SYS_GETRESGID   22
#define SYS_CLOCK_GET   23
#define SYS_GETPID      24
#define SYS_GETPPID     25
#define SYS_GETHOSTNAME 26
#define SYS_FSTATAT     27
#define SYS_GETPGID     28

#define SYS_HOS_SET_KB_LAYOUT   100

uint64_t syscall0_1(uint64_t calln);
uint64_t syscall0_2(uint64_t calln, uint64_t* r1);
uint64_t syscall1_1(uint64_t calln, 
    uint64_t a1);
uint64_t syscall1_2(uint64_t calln, 
    uint64_t a1, uint64_t* r1);
uint64_t syscall2_1(uint64_t calln, 
    uint64_t a1, uint64_t a2);
uint64_t syscall3_1(uint64_t calln, 
    uint64_t a1, uint64_t a2, uint64_t a3);
uint64_t syscall3_2(uint64_t calln, 
    uint64_t a1, uint64_t a2, uint64_t a3, 
    uint64_t* r1);
uint64_t syscall4_1(uint64_t calln, 
    uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4);
uint64_t syscall4_2(uint64_t calln, 
    uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, 
    uint64_t* r1);
uint64_t syscall6_2(uint64_t calln, 
    uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6,
    uint64_t* r1);
