#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

void __assert_fail(const char* assertion, const char* file, unsigned int line, const char* function)
{
    if (function && function[0])
        fprintf(stderr, "%s:%u: %s%sAssertion `%s' failed.\n",
                file, line,
                function, function[0] ? ": " : "",
                assertion);
    else
        fprintf(stderr, "%s:%u: Assertion `%s' failed.\n",
                file, line,
                assertion);

    fflush(stderr);
    abort();
    __builtin_unreachable();
}
