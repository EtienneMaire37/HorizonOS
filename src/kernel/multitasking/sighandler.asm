bits 64
section .sigwrap

; * rax = sighandler
global sighandler
sighandler:
    call rax

    mov rax, 33
    syscall
do_ud:
    ud
    jmp do_ud

extern intret
global sigret
sigret: 
    mov rsp, [gs:0] ; * remove syscall stack frame
    jmp intret