#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "../util/assert.h"
#include "../multicore/spinlock.h"

// Eg: DEFINE_REFCOUNT_INC1(teststruct)
//     DEFINE_REFCOUNT_INC2(teststruct, __teststruct_create_params_t)
//     DEFINE_REFCOUNT_IMPL(teststruct, __teststruct_create, __teststruct_free, __teststruct_create_params_t)
#define DEFINE_REFCOUNT_INC1(struct_name) \
typedef struct struct_name##_counter struct_name##_counter_t; \
typedef struct \
{ \
    struct_name##_counter_t* ptr; \
} struct_name##_strong_ref_t; \
typedef struct \
{ \
    struct_name##_counter_t* ptr; \
} struct_name##_weak_ref_t; \
typedef struct struct_name##_counter \
{ \
    int strong, total; \
    void (*free)(struct_name##_strong_ref_t); \
    struct_name##_t* ptr; \
    bool freed; \
    spinlock_noint_t lock; \
} struct_name##_counter_t;
#define DEFINE_REFCOUNT_INC2(struct_name, create_params) \
struct_name##_strong_ref_t struct_name##_create(create_params params);  \
struct_name##_strong_ref_t struct_name##_weak_deref(struct_name##_weak_ref_t ref); \
struct_name##_strong_ref_t struct_name##_dup_strong_ref(struct_name##_strong_ref_t ref); \
void struct_name##_refdec(struct_name##_strong_ref_t* ref); \
void struct_name##_delete_weak_ref(struct_name##_weak_ref_t* ref); \
void struct_name##_delete_strong_ref(struct_name##_strong_ref_t* ref);
#define DEFINE_REFCOUNT_IMPL(struct_name, struct_create, struct_free, create_params) \
struct_name##_strong_ref_t struct_name##_create(create_params params)  \
{ \
    struct_name##_counter_t* ts_counter = (struct_name##_counter_t*)malloc(sizeof(*ts_counter)); \
    if (!ts_counter) \
        return (struct_name##_strong_ref_t){ NULL }; \
    struct_name##_t* ts = struct_create(params); \
    if (!ts) return (struct_name##_strong_ref_t){ NULL }; \
    ts_counter->free = struct_free; \
    ts_counter->lock = SPINLOCK_NOINT_INIT; \
    ts_counter->ptr = ts; \
    ts_counter->strong = ts_counter->total = 1; \
    ts_counter->freed = false; \
    struct_name##_strong_ref_t ref = { ts_counter }; \
    return ref; \
} \
struct_name##_strong_ref_t struct_name##_weak_deref(struct_name##_weak_ref_t ref) \
{ \
    if (!ref.ptr) return (struct_name##_strong_ref_t){ NULL }; \
    uint32_t flags = acquire_spinlock_noint(&ref.ptr->lock); \
    if (ref.ptr->ptr) \
    { \
        ASSERT(!ref.ptr->freed); \
        ref.ptr->strong++; \
        ref.ptr->total++; \
    } \
    release_spinlock_noint(&ref.ptr->lock, flags); \
    return (struct_name##_strong_ref_t){ ref.ptr }; \
} \
struct_name##_strong_ref_t struct_name##_dup_strong_ref(struct_name##_strong_ref_t ref) \
{ \
    if (!ref.ptr) return (struct_name##_strong_ref_t){ NULL }; \
    uint32_t flags = acquire_spinlock_noint(&ref.ptr->lock); \
    ASSERT(!ref.ptr->freed); \
    ref.ptr->strong++; \
    ref.ptr->total++; \
    release_spinlock_noint(&ref.ptr->lock, flags); \
    return ref; \
} \
void struct_name##_delete_weak_ref(struct_name##_weak_ref_t* ref) \
{ \
    if (!ref) return; \
    if (!ref->ptr) return; \
    uint32_t flags = acquire_spinlock_noint(&ref->ptr->lock); \
    ref->ptr->total--; \
    if (ref->ptr->total <= 0) \
    { \
        free(ref->ptr); \
        return; \
    } \
    release_spinlock_noint(&ref->ptr->lock, flags); \
    ref->ptr = NULL; \
} \
void struct_name##_refdec(struct_name##_strong_ref_t* ref) \
{ \
    uint32_t flags = acquire_spinlock_noint(&ref->ptr->lock); \
    ref->ptr->strong--; \
    ref->ptr->total--; \
    if (ref->ptr->strong <= 0) \
    { \
        ref->ptr->freed = true; \
        ref->ptr->free(*ref); \
        ref->ptr->ptr = NULL; \
    } \
    if (ref->ptr->total <= 0) \
    { \
        free(ref->ptr); \
        return; \
    } \
    release_spinlock_noint(&ref->ptr->lock, flags); \
} \
void struct_name##_delete_strong_ref(struct_name##_strong_ref_t* ref) \
{ \
    if (!ref) return; \
    if (!ref->ptr) return; \
    struct_name##_refdec(ref); \
    ref->ptr = NULL; \
}
