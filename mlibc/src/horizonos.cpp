#include "stub.h"
#include "syscall.hpp"

extern "C" int set_kb_layout(int layout_idx)
{
	return syscall1_1(SYS_HOS_SET_KB_LAYOUT, (uint64_t)layout_idx);
}