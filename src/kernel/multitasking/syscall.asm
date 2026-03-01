bits 64
section .text

extern c_syscall_handler

global syscall_handler
syscall_handler:
    cld

    swapgs

    mov [gs:0], rsp
    mov rsp, [gs:8]

    ; * pad rsp to match the stack frame of interrupt_handler
    push 0x18 | 3       ; * ss
    push qword [gs:0]   ; * user rsp
    push r11            ; * rflags
    push 0x20 | 3       ; * cs
    push rcx            ; * rip
    sub rsp, 8 + 8

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

    xor rax, rax
    mov ax, ds
    push rax

    mov ax, ss
    mov ds, ax

    sub rsp, 8 + 8  ; * see above

    mov rdi, rsp
    lea rsi, [rsp - 8]

    sti
    call c_syscall_handler
    cli

    add rsp, 8 + 8 ; * same here

    pop rax
    mov ds, ax

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

    add rsp, 8 + 8 ; * same here
    add rsp, 8 * 5

    mov rsp, [gs:0]

    swapgs

    o64 sysret