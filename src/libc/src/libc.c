#define BUILDING_C_LIB

char** environ;
static int num_environ;

#include <stdbool.h>
// #include <stdint.h>
#include <stdatomic.h>

#include <inttypes.h>

#include "math_utils.h"

#include "../../kernel/multicore/spinlock.h"
#include "syscall_defines.h"
#include <limits.h>
#include <sys/types.h>

#include "../include/errno.h"
#include "string.c"
#include "../include/string.h"
#include "../include/fcntl.h"
#include "../include/unistd.h"
#include "../include/stdio.h"
#include "../include/sys/wait.h"
#include "../include/stdlib.h"
#include "../include/time.h"
#include "../include/stdarg.h"
#include <sys/types.h>
#include "../include/horizonos.h"
#include "../include/sys/stat.h"
#include "../include/dirent.h"
#include "../include/termios.h"

#include "misc.h"

#include "../../kernel/multitasking/startup_data.h"

FILE* FILE_create()
{
    FILE* f = (FILE*)malloc(sizeof(FILE));
    if (!f) return NULL;
    f->buffer = (uint8_t*)malloc(BUFSIZ);
    if (!f->buffer)
    {
        free(f);
        return NULL;
    }
    f->fd = -1;
    f->buffer_size = BUFSIZ;
    f->buffer_index = 0;
    f->buffer_mode = 0;
    f->flags = FILE_FLAGS_BF_ALLOC;
    f->current_flags = 0;
    f->buffer_end_index = 0;
    return f;
}

#include "unistd.c"
#include "stdio.c"
#include "stdlib.c"
#include "time.c"
#include "dirent.c"

#include "../../liballoc/liballoc.h"
#include "liballoc_hooks.c"
#include "../../liballoc/liballoc.c"

#include "kernel_glue.c"
#include "horizonos.c"

#include "crt0.c"