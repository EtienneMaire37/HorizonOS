#pragma once

#include "../time/time.h"
#include "../time/ktime.h"
#include "../util/evaluate.h"

#include "../io/io.h"
#include <stdbool.h>
#include <inttypes.h>

#define LOG_LEVEL_TRACE     0
#define LOG_LEVEL_DEBUG     1
#define LOG_LEVEL_INFO      2
#define LOG_LEVEL_WARNING   3
#define LOG_LEVEL_ERROR     4
#define LOG_LEVEL_CRITICAL  5

#define TRACE               LOG_LEVEL_TRACE
#define DEBUG               LOG_LEVEL_DEBUG
#define INFO                LOG_LEVEL_INFO
#define WARNING             LOG_LEVEL_WARNING
#define ERROR               LOG_LEVEL_ERROR
#define CRITICAL            LOG_LEVEL_CRITICAL
#define NO_LOGS             (CRITICAL + 1)

static inline void debug_outc(char c)
{
    outb(0xe9, c);
}

static const char* LOG_LEVEL_STR[] =
{
    "[Trace]",
    "[Debug]",
    "[Info]",
    "[Warn]",
    "[Error]",
    "[Fatal]"
};

static const char* LOG_LEVEL_COLOR[] =
{
    "",
    "",
    "",
    "",
    "",
    ""
};

extern bool first_log;

#define LOG_FMT         "%12" PRIu64 ".%03" PRIu64 " - %44s:%4d - %s%s \t"

#define LOG_FMT_NOTIME  "____________.___ - %44s:%4d - %s%s \t"

#ifndef LOG_LEVEL
#define LOG_LEVEL NO_LOGS
#endif
#define _LOG(level, ...) do { \
    if (!first_log) fputc('\n', stderr); \
    first_log = false; \
    uint32_t flags = acquire_spinlock_noint(&time_lock); \
    __resolve_time(); \
    release_spinlock_noint(&time_lock, flags); \
    if (time_initialized) \
        fprintf(stderr, LOG_FMT, \
            (uint64_t)current_time.tv_sec, (uint64_t)current_time.tv_nsec / 1000000, \
            __FILE__, __LINE__, \
            LOG_LEVEL_COLOR[level], LOG_LEVEL_STR[level]); \
    else fprintf(stderr, LOG_FMT_NOTIME, __FILE__, __LINE__, LOG_LEVEL_COLOR[level], LOG_LEVEL_STR[level]); \
    fprintf(stderr, __VA_ARGS__); \
} while (0)

#define LOG(level, ...) \
    do { int logl = (level); if (LOG_LEVEL <= logl && LOG_LEVEL >= 0 && (size_t)LOG_LEVEL < sizeof(LOG_LEVEL_STR) / sizeof(char*)) _LOG(level, __VA_ARGS__); else evaluate(__VA_ARGS__); } while(0)

#define CONTINUE_LOG(level, ...) \
    do { int logl = (level); if (LOG_LEVEL <= logl && LOG_LEVEL >= 0 && (size_t)LOG_LEVEL < sizeof(LOG_LEVEL_STR) / sizeof(char*)) fprintf(stderr, __VA_ARGS__); else evaluate(__VA_ARGS__); } while(0)
