#include <errno.h>
#include <stdint.h>

uint64_t syscall1_1(uint64_t calln, uint64_t a1)
{
    uint64_t ret;
    asm volatile ("syscall" : "=a"(ret) : "a"(calln), "b"(a1) : "memory", "r11", "rcx");
    return ret;
}