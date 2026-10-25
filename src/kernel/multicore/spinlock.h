#pragma once

#include <stdatomic.h>
#include <stdbool.h>
#include "../cpu/registers.h"
#include "../cpu/util.h"
#include "../util/error.h"

typedef struct
{
    atomic_flag flag;
} spinlock_noint_t;
typedef struct
{
    atomic_flag flag;
} spinlock_t;


#define SPINLOCK_INIT           ((spinlock_t){ ATOMIC_FLAG_INIT })
#define SPINLOCK_NOINT_INIT     ((spinlock_noint_t){ ATOMIC_FLAG_INIT })

static inline __attribute__((always_inline)) bool try_acquire_spinlock(spinlock_t* spinlock)
{
	return atomic_flag_test_and_set_explicit(&spinlock->flag, memory_order_acquire);
}

static inline __attribute__((always_inline)) void acquire_spinlock(spinlock_t* spinlock)
{
	while (try_acquire_spinlock(spinlock))
    {
        FATAL("DEADLOCK");
        __builtin_ia32_pause();
    }
}

static inline __attribute__((always_inline)) void release_spinlock(spinlock_t* spinlock)
{
	atomic_flag_clear_explicit(&spinlock->flag, memory_order_release);
}

static inline bool try_acquire_spinlock_noint(spinlock_noint_t* spinlock, uint32_t* flags)
{
    uint32_t eflags = get_rflags();
    *flags = eflags;
    disable_interrupts();
    if (eflags & (1 << 9))
    {
        if (try_acquire_spinlock((spinlock_t*)spinlock))
        {
            enable_interrupts();
            return true;
        }
        enable_interrupts();
        return false;
    }
    return try_acquire_spinlock((spinlock_t*)spinlock);
}

static inline uint32_t acquire_spinlock_noint(spinlock_noint_t* spinlock)
{
    uint32_t eflags = get_rflags();
    disable_interrupts();
    while (try_acquire_spinlock((spinlock_t*)spinlock))
    {
        FATAL("DEADLOCK");
        set_rflags_if(eflags);
        __builtin_ia32_pause();
        disable_interrupts();
    }
    return eflags;
}

static inline void release_spinlock_noint(spinlock_noint_t* spinlock, uint32_t eflags)
{
	atomic_flag_clear_explicit(&spinlock->flag, memory_order_release);
	set_rflags_if(eflags);
}
