bits 64
section .sigwrap

; * rax = sighandler
global sighandler
sighandler:
    call rax

    mov rax, 33 ; * sigret
    syscall
do_ud:
    ud2
    jmp do_ud

extern intret
global sigret
sigret:
    mov rsp, rax ; * remove syscall stack frame
    jmp intret
