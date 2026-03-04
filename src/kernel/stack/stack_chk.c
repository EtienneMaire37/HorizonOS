#include "../int/kernel_panic.h"

uintptr_t __attribute__((used)) __stack_chk_guard = 0xdeadcafedeadcafe;

void __attribute__((noreturn, used)) __stack_chk_fail()
{
    kernel_panic_ex(NULL, PANIC_STCK_CHK_FAIL);
}