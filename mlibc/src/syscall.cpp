#include <errno.h>
#include <stdint.h>

uint64_t syscall1_1(uint64_t calln, 
    uint64_t a1)
{
    uint64_t ret;
    asm volatile ("syscall" : "=a"(ret) : "a"(calln), "b"(a1) : "memory", "r11", "rcx");
    return ret;
}

uint64_t syscall3_2(uint64_t calln, 
    uint64_t a1, uint64_t a2, uint64_t a3, 
    uint64_t* r1)
{
    uint64_t ret;
    asm volatile ("syscall" : "=a"(ret), "=b"(*r1) : "a"(calln), "b"(a1), "d"(a2), "S"(a3) : "memory", "r11", "rcx");
    return ret;
}

uint64_t syscall6_2(uint64_t calln, 
    uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6,
    uint64_t* r1)
{
    // * "Basically you can't specify r8-r15 as constraints directly, 
    // * so you do it indirectly by using local register variables. 
    // * The generated output is what you would expect (i.e. it works)."
    register int64_t r8 asm("r8") = a5;
    register int64_t r9 asm("r9") = a6;

    uint64_t ret;
    asm volatile ("syscall" : "=a"(ret), "=b"(*r1) : "a"(calln), "b"(a1), "d"(a2), "S"(a3), "D"(a4), "r"(a5), "r"(a6) : "memory", "r11", "rcx");
    return ret;
}