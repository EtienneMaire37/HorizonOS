#include <stddef.h>
#include <stdint.h>

#include "../../liballoc/liballoc.c"
#include "../cpu/util.h"
#include "../debug/out.h"

void __attribute__((noreturn)) abort()
{
    disable_interrupts();
    LOG(ERROR, "Kernel aborted.");
    halt();
}