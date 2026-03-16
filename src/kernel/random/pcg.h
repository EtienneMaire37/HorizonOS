#pragma once

#include <stdint.h>

// * https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/
static inline uint32_t pcg_hash(uint32_t input)
{
    uint32_t state = input * 747796405 + 2891336453;
    uint32_t word = ((state >> ((state >> 28) + 4)) ^ state) * 277803737;
    return (word >> 22) ^ word;
}
extern uint32_t pcg_state;
static inline void srand_pcg(uint32_t seed)
{
    pcg_state = seed;
}
static inline uint32_t rand_pcg()
{
    uint32_t state = pcg_state;
    pcg_state = pcg_state * 747796405 + 2891336453;
    uint32_t word = ((state >> ((state >> 28) + 4)) ^ state) * 277803737;
    return (word >> 22) ^ word;
}