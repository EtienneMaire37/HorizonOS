bits 64
section .text

default rel

extern _main
global _start
_start:
    mov [kernel_data], rsp
    mov rbp, 0
    call .start ; stack frame
    jmp $
.start:
    push 0
    mov rbp, 0

    and rsp, 0xfffffffffffffff0

    call _main

    jmp $

extern main
extern exit

global call_main_exit
call_main_exit:
    mov rbp, 0
    push rbp

    and rsp, 0xfffffffffffffff0

    ; ? arguments are passed as registers, no need to set them up
    call main

    mov rdi, rax ; * main return value
    call exit

    jmp $

section .data

global kernel_data
kernel_data: dq 0