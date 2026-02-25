#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "../debug/out.h"

void __assert_fail(const char* assertion, const char* file, unsigned int line, const char* function)
{
    if (function && function[0])
    {
        printf("%s:%u: %s%sAssertion `%s' failed.\n",
                file, line,
                function, function[0] ? ": " : "",
                assertion);
        LOG(CRITICAL, "%s:%u: %s%sAssertion `%s' failed.",
                file, line,
                function, function[0] ? ": " : "",
                assertion);
    }
    else
    {
        printf("%s:%u: Assertion `%s' failed.\n",
                file, line,
                assertion);
        LOG(CRITICAL, "%s:%u: Assertion `%s' failed.",
                file, line,
                assertion);
    }

    fflush(stdout);
    abort();
    __builtin_unreachable();
}
