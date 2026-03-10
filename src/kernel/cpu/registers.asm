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

global set_rflags_if
set_rflags_if:
    pushfq
    pop rsi
    and rsi, ~(1 << 9)
    and rdi, (1 << 9)
    or rdi, rsi

    push rdi
    popfq
    ret