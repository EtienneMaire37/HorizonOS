#pragma once

#include <assert.h>
#include "cfunc.h"

#define FATAL(msg) __assert_fail(msg, __FILE__, __LINE__, __CURRENT_FUNC__)
