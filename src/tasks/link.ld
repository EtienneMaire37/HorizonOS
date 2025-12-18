ENTRY(_start)

OUTPUT_FORMAT(elf64-x86-64)

PHDRS 
{
    text PT_LOAD    FLAGS(5); /* 101 : READABLE | EXECUTABLE */
    data PT_LOAD    FLAGS(6); /* 110 : READABLE | WRITABLE */
    bss PT_LOAD     FLAGS(6); /* 110 : READABLE | WRITABLE */
}

SECTIONS 
{
    . = 0x10000000000;  /* 1 TB */

    .text ALIGN(0x1000) : 
    {
        *(.text .text.*)
    } :text

    .rodata ALIGN(0x1000) : 
    {
        *(.rodata .rodata.*)
    } :text

    .data ALIGN(0x1000) : 
    {
        *(.data .data.*)
    } :data

    .bss ALIGN(0x1000) : 
    {
        *(COMMON)
        *(.bss .bss.*)
    } :bss

    _end = .;
    
    /DISCARD/ : { *(.comment) *(.note) }
}