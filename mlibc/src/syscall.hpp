#pragma once

#define SYS_SETFS   0
#define SYS_WRITE   1
#define SYS_EXIT    2
#define SYS_ISATTY  3
#define SYS_VM_MAP  4

uint64_t syscall1_1(uint64_t calln, 
    uint64_t a1);
uint64_t syscall3_2(uint64_t calln, 
    uint64_t a1, uint64_t a2, uint64_t a3, 
    uint64_t* r1);
uint64_t syscall6_2(uint64_t calln, 
    uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6,
    uint64_t* r1);