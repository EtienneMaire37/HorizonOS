bits 64
section .sigwrap

; void __attribute__((section(".sigwrap"), used)) sighandler()
; {
;     const char str[] = "sighandler";
;     uint64_t r64;
;     int ret;
;     asm volatile ("syscall" : "=a"(ret), "=b"(r64) : "a"(SYS_WRITE), 
;     "b"(STDOUT_FILENO), "d"(str), "S"(sizeof(str)) : "memory", "r11", "rcx");
    
;     asm volatile ("syscall" :: "a"(SYS_SIGRET));
; }

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