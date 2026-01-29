#pragma once

#include "../src/current_func.h"

#ifndef NULL
#define NULL ((void*)0)
#endif

#define RAND_MAX 32767
#define EXIT_FAILURE 127
#define EXIT_SUCCESS 0
#define MB_CUR_MAX   1  // Only ASCII is supported

#include <bits/size_t.h>
#include <bits/wchar_t.h>

void exit(int r);
int rand();
void srand(unsigned int seed);

long a64l(const char*s);
char* l64a(long value);
int abs(int n);
int atexit(void (*function)());
int isdigit(int c);
int isspace(int c);
int isalpha(int c);
int tolower(int c);
int toupper(int c);
int atoi(const char* str);
#ifdef BUILDING_KERNEL
void __attribute__((noreturn)) abort_core(int line, const char* file, const char* function);
#define abort() abort_core(__LINE__, __FILE__, __CURRENT_FUNC__)
#else
void __attribute__((noreturn)) abort();
#endif
long strtol(const char* nptr, char** endptr, int base);
int atoi(const char* str);

// ~ From liballoc.h
void     *malloc(size_t);				//< The standard function.
void     *realloc(void *, size_t);		//< The standard function.
void     *calloc(size_t, size_t);		//< The standard function.
void      free(void *);				//< The standard function

char* getenv(const char* name);
int setenv(const char* name, const char* value, int overwrite);
int unsetenv(const char* name);

int system(const char* command);

char* realpath(const char* path, char* resolved_path);