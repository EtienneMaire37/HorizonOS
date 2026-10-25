#include "error.h"
#include <stdio.h>
#include <string.h>

void pstrerror(int _errno, const char* str)
{
    printf("%s: %s", str, strerror(_errno));
}

