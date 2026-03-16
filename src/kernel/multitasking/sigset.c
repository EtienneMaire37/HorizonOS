#include "sigset.h"

sigset_t sigset_bitwise_or(sigset_t s1, sigset_t s2)
{
	sigset_t r;
	for (size_t i = 0; i < sizeof(sigset_t); i++)
		((uint8_t*)&r)[i] = ((uint8_t*)&s1)[i] | ((uint8_t*)&s2)[i];
	return r;
}

sigset_t sigset_bitwise_and(sigset_t s1, sigset_t s2)
{
	sigset_t r;
	for (size_t i = 0; i < sizeof(sigset_t); i++)
		((uint8_t*)&r)[i] = ((uint8_t*)&s1)[i] & ((uint8_t*)&s2)[i];
	return r;
}

sigset_t sigset_bitwise_not(sigset_t s)
{
	sigset_t r;
	for (size_t i = 0; i < sizeof(sigset_t); i++)
		((uint8_t*)&r)[i] = ~((uint8_t*)&s)[i];
	return r;
}

bool sigset_is_bit_set(sigset_t s, int bit)
{
	int idx = bit / sizeof(unsigned long);
    return (s.__sig[idx] & (1ULL << (bit - idx * sizeof(unsigned long)))) != 0;
}