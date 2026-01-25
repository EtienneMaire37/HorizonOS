bits 64
section .text

global syscall_handler
syscall_handler:
    swapgs

    mov gs:0, rsp
    mov rsp, gs:8

    ; 

    mov rsp, gs:0

    swapgs

    o64 sysret