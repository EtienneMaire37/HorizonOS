#include "stub.h"

extern "C" bool set_kb_layout(int layout_idx)
{
	(void)layout_idx;
	STUB("set_kb_layout");
}