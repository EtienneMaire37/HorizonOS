#pragma once

#include "stdarg.h"

#ifndef NULL
#define NULL ((void*)0)
#endif

#include <bits/size_t.h>

#define EOF (-1)

#include "seek_defs.h"

#define BUFSIZ 4096

typedef unsigned int fpos_t;

#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2

#define FILENAME_MAX    255
#define FOPEN_MAX       16
#define TMP_MAX         65536

typedef struct FILE FILE;
struct FILE
{
    int fd;
    unsigned char* buffer;
    size_t buffer_size;
    size_t buffer_index, buffer_end_index;
    unsigned char buffer_mode;
    unsigned char flags;
    unsigned char current_flags;
};

#define FILE_FLAGS_READ     0x01
#define FILE_FLAGS_WRITE    0x02

#define FILE_FLAGS_FBF      0x04
#define FILE_FLAGS_LBF      0x08
#define FILE_FLAGS_NBF      0x10

#define FILE_FLAGS_BF_ALLOC 0x20


#define FILE_CFLAGS_EOF     0x01
#define FILE_CFLAGS_ERR     0x02

#define FILE_BFMD_READ      0
#define FILE_BFMD_WRITE     1

#ifdef BUILDING_C_LIB
FILE* stdin;
FILE* stdout;
FILE* stderr;
#else
extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;
#endif

FILE* fopen(const char* path, const char* mode);
int fclose(FILE* stream);
size_t fread(void* ptr, size_t size, size_t nitems, FILE* stream);
size_t fwrite(const void* ptr, size_t size, size_t nitems, FILE* stream);
int fflush(FILE *stream);
int feof(FILE* stream);
int ferror(FILE* stream);
int fgetc(FILE* stream);
int fputc(int c, FILE* stream);
int getchar();

void perror(const char* prefix);

#define putc fputc
#define getc fgetc

int putchar(int c);
int fputs(const char* s, FILE* stream);
int puts(const char* s);

int printf(const char* format, ...);
int fprintf(FILE* stream, const char* format, ...);
int vfprintf(FILE* stream, const char* format, va_list args);
int vprintf(const char* format, va_list args);
int dprintf(int fd, const char*format, ...);
int vdprintf(int fd, const char*format, va_list args);
int sprintf(char* buffer, const char* format, ...);
int snprintf(char* buffer, size_t bufsz, const char* format, ...);
int vsprintf(char* buffer, const char* format, va_list args);
int vsnprintf(char* buffer, size_t bufsz, const char* format, va_list args);