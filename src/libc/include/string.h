#pragma once

#ifndef NULL
#define NULL ((void*)0)
#endif

#include <bits/size_t.h>

// void* memcpy(void* dst, const void* src, size_t n);
// void* memset(void* dst, int c, size_t n);
// int memcmp(const void* a, const void* b, size_t n);
// void* memmove(void* dst, const void* src, size_t length);

#undef memcpy
#undef memset
#undef memcmp
#undef memmove

#define memcpy __builtin_memcpy
#define memset __builtin_memset
#define memcmp __builtin_memcmp
#define memmove __builtin_memmove

// size_t strlen(const char* s);
// int strcmp(const char* a, const char* b);
// char* strcpy(char* dst, const char* src);
// char* strncpy(char* dst, const char* src, size_t n);

#undef strlen
#undef strcmp
#undef strcpy
#undef strncpy

#define strlen __builtin_strlen
#define strcmp __builtin_strcmp
#define strcpy __builtin_strcpy
#define strncpy __builtin_strncpy

char* strdup(const char* str);
char* strerror(int errnum);
char* strsignal(int signum);