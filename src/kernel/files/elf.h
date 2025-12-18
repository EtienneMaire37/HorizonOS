#pragma once

typedef uint16_t    elf64_half_t;
typedef uint32_t    elf64_word_t;
typedef uint64_t    elf64_addr_t;
typedef uint64_t    elf64_off_t;
typedef int32_t     elf64_sword_t;

#define ELF_NIDENT 16

#define ELF_CLASS_32    1
#define ELF_CLASS_64    2

#define ELF_DATA_LITTLE_ENDIAN  1
#define ELF_DATA_BIG_ENDIAN     2

#define ELF_OSABI_SYSV  0

#define ELF_INSTRUCTION_SET_x86     0x03
#define ELF_INSTRUCTION_SET_x86_64  0x3E

#define ELF_TYPE_RELOCATABLE    1
#define ELF_TYPE_EXECUTABLE     2
#define ELF_TYPE_SHARED         3
#define ELF_TYPE_CORE           4

typedef struct __attribute__((packed)) elf64_header
{
    uint8_t         magic[4];
    uint8_t         architecture;
    uint8_t         byte_order;
    uint8_t         header_version;
    uint8_t         osabi;
    uint8_t         abiversion;
    uint8_t         pad[7];
    
    elf64_half_t    type;
    elf64_half_t    machine;
    elf64_word_t    elf_version;
    elf64_addr_t    entry;
    elf64_off_t     phoff;
    elf64_off_t     shoff;
    elf64_word_t    flags;
    elf64_half_t    ehsize;
    elf64_half_t    phentsize;
    elf64_half_t    phnum;
    elf64_half_t    shentsize;
    elf64_half_t    shnum;
    elf64_half_t    shstrndx;
} elf64_header_t;

typedef struct __attribute__((packed)) elf64_program_header
{
    elf64_word_t    type;
    elf64_word_t    flags;
    elf64_off_t     p_offset;
    elf64_addr_t    p_vaddr;
    elf64_addr_t    p_paddr;
    elf64_off_t     p_filesz;
    elf64_off_t     p_memsz;
    elf64_off_t     align;
} elf64_program_header_t;

typedef struct __attribute__((packed)) elf64_section_header
{
    elf64_word_t    name;
    elf64_word_t    type;
    elf64_off_t     flags;
    elf64_addr_t    addr;
    elf64_off_t     offset;
    elf64_off_t     size;
    elf64_word_t    link;
    elf64_word_t    info;
    elf64_off_t     addralign;
    elf64_off_t     entsize;
} elf64_section_header_t;

#define ELF_PROGRAM_TYPE_NULL       0
#define ELF_PROGRAM_TYPE_LOAD       1
#define ELF_PROGRAM_TYPE_DYNAMIC    2
#define ELF_PROGRAM_TYPE_INTERP     3
#define ELF_PROGRAM_TYPE_NOTE       4

const char* elf_program_header_type_string[] = 
{
    "NULL",
    "LOAD",
    "DYNAMIC",
    "INTERP",
    "NOTE"
};

#define ELF_SECTION_TYPE_NULL           0x00
#define ELF_SECTION_TYPE_PROGBITS       0x01
#define ELF_SECTION_TYPE_SYMTAB         0x02
#define ELF_SECTION_TYPE_STRTAB         0x03
#define ELF_SECTION_TYPE_RELA           0x04
#define ELF_SECTION_TYPE_HASH           0x05
#define ELF_SECTION_TYPE_DYNAMIC        0x06
#define ELF_SECTION_TYPE_NOTE           0x07
#define ELF_SECTION_TYPE_NOBITS         0x08
#define ELF_SECTION_TYPE_REL            0x09
#define ELF_SECTION_TYPE_SHLIB          0x0a
#define ELF_SECTION_TYPE_DYNSYM         0x0b
#define ELF_SECTION_TYPE_INIT_ARRAY     0x0e
#define ELF_SECTION_TYPE_FINI_ARRAY     0x0f
#define ELF_SECTION_TYPE_PREINIT_ARRAY  0x10
#define ELF_SECTION_TYPE_GROUP          0x11
#define ELF_SECTION_TYPE_SYMTAB_SHNDX   0x12
#define ELF_SECTION_TYPE_NUM            0x13

const char* elf_section_header_type_string[] =
{
    "NULL",
    "PROGBITS",
    "SYMTAB",
    "STRTAB",
    "RELA",
    "HASH",
    "DYNAMIC",
    "NOTE",
    "NOBITS",
    "REL",
    "SHLIB",
    "DYNSYM",
    "UNKNOWN",
    "UNKNOWN",
    "INIT_ARRAY",
    "FINI_ARRAY",
    "PREINIT_ARRAY",
    "GROUP",
    "SYMTAB_SHNDX"
};

#define ELF_SECTION_FLAG_WRITE      1
#define ELF_SECTION_FLAG_ALLOC      2

#define ELF_FLAG_EXECUTABLE         1
#define ELF_FLAG_WRITABLE           2
#define ELF_FLAG_READABLE           4

const char* elf64_get_phtype_string(elf64_word_t type)
{
    return type >= sizeof(elf_program_header_type_string) / sizeof(char*) ? "UNKNOWN" : elf_program_header_type_string[type];
}

const char* elf64_get_shtype_string(elf64_word_t type)
{
    return type >= sizeof(elf_section_header_type_string) / sizeof(char*) ? "UNKNOWN" : elf_section_header_type_string[type];
}