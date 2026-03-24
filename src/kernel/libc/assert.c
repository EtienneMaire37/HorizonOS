#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "../debug/out.h"
#include "../multitasking/multitasking.h"

void __assert_fail(const char* assertion, const char* file, unsigned int line, const char* function)
{
    pid_t pid = multitasking_enabled ? current_task->pid : -1;
    if (function && function[0])
    {
        printf("[pid = %d] %s:%u: %s%sAssertion `%s' failed.\n",
                pid,
                file, line,
                function, function[0] ? ": " : "",
                assertion);
        LOG(CRITICAL, "[pid = %d] %s:%u: %s%sAssertion `%s' failed.",
                pid,
                file, line,
                function, function[0] ? ": " : "",
                assertion);
    }
    else
    {
        printf("[pid = %d] %s:%u: Assertion `%s' failed.\n",
                pid,
                file, line,
                assertion);
        LOG(CRITICAL, "[pid = %d] %s:%u: Assertion `%s' failed.",
                pid,
                file, line,
                assertion);
    }

    fflush(stdout);
    abort();
    __builtin_unreachable();
}
