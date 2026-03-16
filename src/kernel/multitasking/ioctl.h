#pragma once

#include "syscall.h"

void syscall_ioctl(interrupt_registers_t* registers, int fd, unsigned long request, void* arg);