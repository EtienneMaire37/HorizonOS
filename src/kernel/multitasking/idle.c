#include "idle.h"
#include <stdbool.h>

void idle_main()
{
    // TODO: Improve scheduler to refresh TSC deadlines every 20ms or so and keep track of time slices
    while (true)
        hlt();
    __builtin_unreachable();
}
