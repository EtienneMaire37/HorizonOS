#include "preempt.h"
#include "multitasking.h"
#include "task.h"

// * Per CPU
int _Atomic preempt_disable_depth = 0;

void recursive_preempt_disable()
{
    preempt_disable_depth++;
}
void recursive_preempt_enable()
{
    preempt_disable_depth--;
    if (queued_ts && multitasking_enabled)
    {
        queued_ts = false;
        switch_task();
    }
}
