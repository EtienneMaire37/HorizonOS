#pragma once

#define likely(expr)                __builtin_expect(!!(expr), 1)
#define unlikely(expr)              __builtin_expect(!!(expr), 0)
#define _likely(expr, prob)         __builtin_expect_with_probability(!!(expr), 1, (prob))
#define _unlikely(expr, prob)       __builtin_expect_with_probability(!!(expr), 0, (prob))
