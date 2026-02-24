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

global sighandler
sighandler:
    mov rax, 1
    mov rbx, 1
    mov rdx, str
    mov rsi, 10
    syscall

    mov rax, 33
    syscall
    ud
    jmp $

str: db "sighandler"