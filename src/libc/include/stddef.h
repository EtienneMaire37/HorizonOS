#pragma once

#ifndef NULL
#define NULL ((void*)0)
#endif

typedef long long ptrdiff_t;
#include <bits/size_t.h>
#include <bits/wchar_t.h>

#define offsetof(st, m) ((size_t)&(((st*)0)->m))