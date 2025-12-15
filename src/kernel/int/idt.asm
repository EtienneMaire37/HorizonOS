bits 64
section .text

idtr:
    dw 0
    dq 0

global load_idt
; void load_idt()
load_idt:
    mov [idtr], di
    mov [idtr + 2], rsi
    lidt [idtr]
    ret

%macro INT_ERROR_CODE 1
INT_%1:
    push qword %1

    jmp _interrupt_handler
%endmacro

%macro INT_NO_ERROR_CODE 1
INT_%1:
    push qword 0
    push qword %1

    jmp _interrupt_handler
%endmacro

; ISRs
INT_NO_ERROR_CODE 0
INT_NO_ERROR_CODE 1
INT_NO_ERROR_CODE 2
INT_NO_ERROR_CODE 3
INT_NO_ERROR_CODE 4
INT_NO_ERROR_CODE 5
INT_NO_ERROR_CODE 6
INT_NO_ERROR_CODE 7
INT_ERROR_CODE    8
INT_NO_ERROR_CODE 9
INT_ERROR_CODE    10
INT_ERROR_CODE    11
INT_ERROR_CODE    12
INT_ERROR_CODE    13
INT_ERROR_CODE    14
INT_NO_ERROR_CODE 15
INT_NO_ERROR_CODE 16
INT_ERROR_CODE    17
INT_NO_ERROR_CODE 18
INT_NO_ERROR_CODE 19
INT_NO_ERROR_CODE 20
INT_NO_ERROR_CODE 21
INT_NO_ERROR_CODE 22
INT_NO_ERROR_CODE 23
INT_NO_ERROR_CODE 24
INT_NO_ERROR_CODE 25
INT_NO_ERROR_CODE 26
INT_NO_ERROR_CODE 27
INT_NO_ERROR_CODE 28
INT_NO_ERROR_CODE 29
INT_ERROR_CODE    30
INT_NO_ERROR_CODE 31

; IRQs
%assign i 32
    %rep    16
        INT_NO_ERROR_CODE i
    %assign i i+1 
    %endrep

; All the other interrupts
%assign i 48
    %rep    (256 - 48)
        INT_NO_ERROR_CODE i
    %assign i i+1 
    %endrep

global interrupt_table
interrupt_table:
    %assign i 0 
    %rep    256
        dq INT_%+i
    %assign i i+1 
    %endrep

extern interrupt_handler
_interrupt_handler:
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

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rax, cr2
    push rax

    mov rax, cr3
    push rax
    
    mov rdi, rsp

    ; * "The common recommendation is to use interrupt gates so that the assembly part of the interrupt handler 
    ; * can fully save all necessary registers to the stack before conditionally enabling interrupts 
    ; * (if "enough" stack space is left for the kernel, for some measure of "enough"). 
    ; * This way, interrupts are enabled most of the time."
    ; * -> nullplan
    sti
    call interrupt_handler
    cli
    
    ; mov rdi, 'A'
    ; call putchar
    ; mov rdi, 10
    ; call putchar
    
    add rsp, 8 + 8  ; skip cr2 and cr3

    pop rax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

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
    
    add rsp, 8 + 8  ; skip error code and interrupt number
global iretq_instruction
iretq_instruction:
    iretq