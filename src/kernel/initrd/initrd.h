#pragma once

#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>

typedef uint8_t tar_file_type;

typedef struct initrd_file
{
    char* name;
    uint8_t* data;
    struct stat st;
    char* link;
} initrd_file_t;

#define MAX_INITRD_FILES 256

extern initrd_file_t initrd_files[MAX_INITRD_FILES];
extern uint16_t initrd_files_count;

void initrd_parse(uint64_t initrd_start, uint64_t initrd_size);
initrd_file_t* initrd_find_file(const char* name);
initrd_file_t* initrd_find_file_entry(const char* name);
