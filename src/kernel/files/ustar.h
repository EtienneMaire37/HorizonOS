#pragma once

#include <stdint.h>

struct __attribute__((packed)) ustar_header
{
    char    name[100];
    uint8_t mode[8];
    uint8_t owner_id[8];
    uint8_t group_id[8];
    char    size[12];
    uint8_t last_modification[12];
    uint8_t checksum[8];
    uint8_t type;
    uint8_t linked_file[100];
    uint8_t ustar[6];
    uint8_t version[2];
    uint8_t owner_name[32];
    uint8_t group_name[32];
    uint8_t device_major[8];
    uint8_t device_minor[8];
    uint8_t filename_prefix[155];
    uint8_t padding[12];
};

#define USTAR_BLOCK_SIZE 512

#define USTAR_TYPE_FILE_1           '0'
#define USTAR_TYPE_FILE_2            0
#define USTAR_TYPE_HARD_LINK        '1'
#define USTAR_TYPE_SYMBOLIC_LINK    '2'
#define USTAR_TYPE_CHARACTER_DEVICE '3'
#define USTAR_TYPE_BLOCK_DEVICE     '4'
#define USTAR_TYPE_DIRECTORY        '5'
#define USTAR_TYPE_NAMED_PIPE       '6'

#define USTAR_IS_VALID_HEADER(header) ((header).ustar[0] == 'u' && (header).ustar[1] == 's' && (header).ustar[2] == 't' && (header).ustar[3] == 'a' && (header).ustar[4] == 'r')

#define TSUID 	04000 	// set user ID on execution
#define TSGID 	02000 	// set group ID on execution
#define TSVTX 	01000 	// reserved
#define TUREAD 	00400 	// read by owner
#define TUWRITE 00200 	// write by owner
#define TUEXEC 	00100 	// execute or search by owner
#define TGREAD 	00040 	// read by group
#define TGWRITE 00020 	// write by group
#define TGEXEC 	00010 	// execute or search by group
#define TOREAD 	00004 	// read by others
#define TOWRITE 00002 	// write by others
#define TOEXEC 	00001 	// execute or search by other

static inline uint64_t ustar_get_number(char* str, int characters)
{
    uint64_t result = 0;
    uint64_t count = 1;

    for (uint8_t j = characters - 1; j > 0; j--, count *= 8)
        result += ((str[j - 1] - '0') * count);

    return result;

}