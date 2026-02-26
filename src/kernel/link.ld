OUTPUT_FORMAT(elf64-x86-64)

ENTRY(_start)

PHDRS
{
    boot PT_LOAD;
    limine_requests PT_LOAD;
    text PT_LOAD;
    rodata PT_LOAD;
    data PT_LOAD;
    sigwrap PT_LOAD;
}

SECTIONS
{
    . = 0xffffffff80000000;
    kernel_start = .;

    .limine_requests : 
    {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    } :limine_requests

    . = ALIGN(4096);

    .text : 
    {
        *(.text .text.*)
    } :text

    . = ALIGN(4096);

    .rodata : 
    {
        *(.rodata .rodata.*)
    } :rodata

    .note.gnu.build-id : 
    {
        *(.note.gnu.build-id)
    } :rodata

    . = ALIGN(4096);

    .data : 
    {
        *(.data .data.*)
    } :data

    . = ALIGN(4096);

    .sigwrap : 
    {
        *(.sigwrap)
    }

    . = ALIGN(4096);

    .bss : 
    {
        *(.bss .bss.*)
        *(COMMON)
    } :data

    . = ALIGN(4096);

    kernel_end = .;

    /DISCARD/ : 
    {
        *(.eh_frame*)
        *(.note .note.*)
    }
}
