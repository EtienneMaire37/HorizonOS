#pragma once

#include <stdint.h>

#define SYS_SETFS           0
#define SYS_WRITE           1
#define SYS_EXIT            2
#define SYS_ISATTY          3
#define SYS_VM_MAP          4
#define SYS_VM_UNMAP        5
#define SYS_SEEK            6
#define SYS_OPEN            7
#define SYS_CLOSE           8
#define SYS_READ            9
#define SYS_IOCTL           10
#define SYS_EXECVE          11
#define SYS_TCSETATTR       12
#define SYS_TCGETATTR       13
#define SYS_GETCWD          14
#define SYS_CHDIR           15
#define SYS_FORK            16
#define SYS_SIGACTION       17
#define SYS_SIGPROCMASK	    18
#define SYS_WAIT4           19
#define SYS_TTYNAME         20
#define SYS_GETRESUID       21
#define SYS_GETRESGID       22
#define SYS_CLOCK_GET       23
#define SYS_GETPID          24
#define SYS_GETPPID         25
#define SYS_GETHOSTNAME     26
#define SYS_FSTATAT         27
#define SYS_GETPGID         28
#define SYS_SETPGID         29
#define SYS_DUP             30
#define SYS_FCNTL           31
#define SYS_DUP3            32
#define SYS_SIGRET          33
#define SYS_PSELECT         34
#define SYS_VM_PROTECT      35
#define SYS_OPEN_DIR        36
#define SYS_READ_ENTRIES    37
#define SYS_ACCESS          38
#define SYS_FADVISE         39
#define SYS_PIPE2           40
#define SYS_KILL            41
#define SYS_UNAME           42
#define SYS_FSYNC           43
#define SYS_SLEEP           44
#define SYS_POLL            45
#define SYS_GETAFFINITY     46

#define SYS_LOG                 0x1000
#define SYS_HOS_SET_KB_LAYOUT   0x2000

uint64_t syscall0_1(uint64_t calln);
uint64_t syscall0_2(uint64_t calln, uint64_t* r1);
void syscall1_0(uint64_t calln,
    uint64_t a1);
uint64_t syscall1_1(uint64_t calln,
    uint64_t a1);
uint64_t syscall1_2(uint64_t calln,
    uint64_t a1, uint64_t* r1);
uint64_t syscall1_3(uint64_t calln,
    uint64_t a1,
    uint64_t* r1, uint64_t* r2);
uint64_t syscall2_1(uint64_t calln,
    uint64_t a1, uint64_t a2);
uint64_t syscall2_2(uint64_t calln,
    uint64_t a1, uint64_t a2,
    uint64_t* r1);
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
