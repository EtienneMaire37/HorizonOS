#include "error.h"
#include "../debug/out.h"
#include <stdio.h>
#include <string.h>

void pstrerror(int _errno, const char* str)
{
    printf("%s: %s", str, strerror(_errno));
    LOG(_errno != 0 ? ERROR : INFO, "%s: %s", str, strerror(_errno));
}

