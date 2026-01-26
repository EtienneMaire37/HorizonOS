bits 64
section .text

extern c_syscall_handler

global syscall_handler
syscall_handler:
    cld

    cli

    swapgs

    mov gs:0, rsp
    mov rsp, gs:8

    sti

    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp
    sub rsp, 8

    call c_syscall_handler

    add rsp, 8

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax

    cli

    mov rsp, gs:0

    swapgs

    o64 sysret