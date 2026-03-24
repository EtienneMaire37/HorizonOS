section .text
bits 64

extern task_rsp_offset
extern task_cr3_offset

; * void context_switch(thread_t* old_tcb, thread_t* next_tcb, uint64_t ds);
    ; $rdi = (uint64_t)old_tcb
    ; $rsi = (uint64_t)next_tcb
    ; $rdx = ds
global context_switch
context_switch:
; * If the callee wishes to use registers RBX, RSP, RBP, and R12–R15,
; * it must restore their original values before returning control to the caller.
; * All other registers must be saved by the caller if it wishes to preserve their values.
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rbp

    mov rbx, qword [rel task_rsp_offset]
    mov [rdi + rbx], rsp                ; rdi->rsp = $rsp

    mov rsp, [rsi + rbx]                ; $rsp = rsi->rsp

    mov rbx, qword [rel task_cr3_offset]

    mov rcx, cr3
    mov [rdi + rbx], rcx
    mov rax, [rsi + rbx]

    cmp rax, rcx
    je .end
    mov cr3, rax

.end:
    pop rbp
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx

    mov ds, dx
    mov es, dx
    mov fs, dx
    mov gs, dx

    ret

global unlock_scheduler__and__iretq
extern unlock_scheduler_symbol
extern iretq_instruction
unlock_scheduler__and__iretq:
    call unlock_scheduler_symbol
    jmp iretq_instruction
