#pragma once

#include "../util/refcount.h"

typedef struct mountpoint mountpoint_t;

DEFINE_REFCOUNT_INC1(mountpoint)

typedef struct
{
    
} mountpoint_create_params_t;

DEFINE_REFCOUNT_INC2(mountpoint, mountpoint_create_params_t)

void ___mountpoint_free(mountpoint_strong_ref_t mp);
mountpoint_t* ___mountpoint_create(mountpoint_create_params_t params);
