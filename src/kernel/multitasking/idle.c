#include "idle.h"

void idle_main()
{
    while(true) 
    {
        // printf("Hello from idle task!!!!\n");
        hlt();
    }
    __builtin_unreachable();
}