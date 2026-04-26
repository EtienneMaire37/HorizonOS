#pragma once

#include "../time/time.h"
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

static inline void debug_outc(char c)
{
    outb(0xe9, c);
}

// #define ANSI_COLORS

static const char* LOG_LEVEL_STR[] =
{
    "[Trace]",
    "[Debug]",
    "[Info]",
    "[Warn]",
    "[Error]",
    "[Fatal]"
};

#ifndef ANSI_COLORS
static const char* LOG_LEVEL_COLOR[] =
{
    "",
    "",
    "",
    "",
    "",
    ""
};
#else
static const char* LOG_LEVEL_COLOR[] =
{
    "\x1b[38;2;130;130;130m\x1b[48;2;30;30;30m",
    "\x1b[38;2;100;150;255m\x1b[48;2;20;30;60m",
    "\x1b[38;2;120;200;120m\x1b[48;2;20;40;20m",
    "\x1b[38;2;255;200;100m\x1b[48;2;60;40;10m",
    "\x1b[38;2;255;100;100m\x1b[48;2;50;10;10m",
    "\x1b[38;2;255;255;255m\x1b[48;2;150;0;0m"
};
#endif

extern bool first_log;

#ifndef ANSI_COLORS
#define LOG_FMT         "%" PRId64 "-%.2" PRId64 "-%.2" PRId64 " \
%.2" PRId64 ":%.2" PRId64 ":%.2" PRId64 ".%.3" PRId64 " - %s%s \t"

#define LOG_FMT_NOTIME  "\
0000-00-00 00:00:00.000\
 - %s%s \t"
#else
#define LOG_FMT "\
\x1b[38;2;200;200;200m\x1b[48;2;40;40;40m%" PRIu64 "-%.2" PRIu64 "-%.2" PRIu64 " \x1b[0m\
\x1b[38;2;150;200;255m\x1b[48;2;40;40;40m%.2" PRIu64 ":%.2" PRIu64 ":%.2" PRIu64 ".%.3llu\x1b[0m\
 - %s%s\x1b[0m \t"

#define LOG_FMT_NOTIME "\
\x1b[38;2;120;120;120m\x1b[48;2;30;30;30m0000-00-00 00:00:00.000\x1b[0m\
 - %s%s\x1b[0m \t"
#endif


#ifndef LOG_LEVEL
#define LOG(level, ...) evaluate(__VA_ARGS__)
#define CONTINUE_LOG(level, ...)
#else
#define _LOG(level, ...) do { \
    if (!first_log) fputc('\n', stderr); \
    first_log = false; \
    if (time_initialized) \
        fprintf(stderr, LOG_FMT, \
            system_year, system_month, system_day, \
            system_hours, system_minutes, system_seconds, \
            system_thousands, \
            LOG_LEVEL_COLOR[level], LOG_LEVEL_STR[level]); \
    else fprintf(stderr, LOG_FMT_NOTIME, LOG_LEVEL_COLOR[level], LOG_LEVEL_STR[level]); \
    fprintf(stderr, __VA_ARGS__); \
} while (0)

#define LOG(level, ...) \
    do { if (LOG_LEVEL <= level && LOG_LEVEL >= 0 && LOG_LEVEL < sizeof(LOG_LEVEL_STR) / sizeof(char*)) _LOG(level, __VA_ARGS__); else evaluate(__VA_ARGS__); } while(0)

#define CONTINUE_LOG(level, ...) \
    do { if (LOG_LEVEL <= level && LOG_LEVEL >= 0 && LOG_LEVEL < sizeof(LOG_LEVEL_STR) / sizeof(char*)) fprintf(stderr, __VA_ARGS__); else evaluate(__VA_ARGS__); } while(0)
#endif
