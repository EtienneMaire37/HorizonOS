#pragma once

#include "../src/current_func.h"

#ifdef NDEBUG
    #define assert() ((void) 0)
#else
    #ifdef BUILDING_KERNEL
        void __attribute__((noreturn)) abort_core(int line, const char* file, const char* function);
        #define abort() abort_core(__LINE__, __FILE__, __CURRENT_FUNC__)
    #else
        void __attribute__((noreturn)) abort();
    #endif

    int dprintf(int fd, const char*format, ...);
    #ifdef BUILDING_KERNEL
        #define assert(val) ((val) ? (void)0 : (fprintf(stdout, "%s: %u: %s: Assertion `%s` failed.\n", __FILE__, (int)__LINE__, __CURRENT_FUNC__, #val), abort()))
    #else
        #define assert(val) ((val) ? (void)0 : (fprintf(stderr, "%s: %u: %s: Assertion `%s` failed.\n", __FILE__, (int)__LINE__, __CURRENT_FUNC__, #val), abort()))
    #endif
#endif