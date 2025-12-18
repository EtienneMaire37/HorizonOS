bits 64
section .text

global get_rflags
get_rflags:
    pushfq
    pop rax
    ret

global set_rflags
set_rflags:
    push rdi
    popfq
    ret